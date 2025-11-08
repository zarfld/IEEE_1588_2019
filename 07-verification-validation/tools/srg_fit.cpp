#include <algorithm>
#include <cmath>
#include <cstdint>
#if __cplusplus >= 201703L
#include <filesystem>
#define SRG_HAVE_FS 1
#else
#define SRG_HAVE_FS 0
#endif
#include <fstream>
#include <iomanip>
#include <limits>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Phase 07: SRG model fitting tool
// Reads SRG CSV exported by Phase 06 (FailureNumber,FailureTime,Severity,Operation,State,Fixed)
// Performs trend tests (Laplace, AM) and fits SRG models (Goel-Okumoto, Musa-Okumoto, Crow/AMSAA)
// Computes SSE, R^2, AIC, current failure intensity and MTBF, and writes an analysis report.

struct Record {
    int failureNumber{};
    double failureTime{}; // duty time or test index
    int severity{};
    std::string operation;
    std::string state;
    bool fixed{};
};

static std::string trim(const std::string &s){
    auto b=s.find_first_not_of(" \t\r\n"); if(b==std::string::npos) return ""; auto e=s.find_last_not_of(" \t\r\n"); return s.substr(b,e-b+1);
}

static std::vector<Record> read_csv(const std::string &path) {
    std::vector<Record> out;
    std::ifstream in(path.c_str());
    if(!in.good()) return out;
    std::string line;
    // header
    if(!std::getline(in,line)) return out;
    while(std::getline(in,line)){
        auto t=trim(line);
        if(t.empty()) continue;
        std::stringstream ss(t);
        std::string col; int idx=0;
        Record r{};
        while(std::getline(ss,col,',')){
            switch(idx){
                case 0: r.failureNumber = std::atoi(col.c_str()); break;
                case 1: r.failureTime = std::atof(col.c_str()); break;
                case 2: r.severity = std::atoi(col.c_str()); break;
                case 3: r.operation = col; break;
                case 4: r.state = col; break;
                case 5: r.fixed = (trim(col)=="1" || trim(col)=="true" || trim(col)=="True"); break;
                default: break;
            }
            ++idx;
        }
        if(r.failureNumber>0) out.push_back(r);
    }
    std::sort(out.begin(), out.end(), [](const Record&a, const Record&b){return a.failureTime < b.failureTime;});
    return out;
}

struct TrendResult { double laplace_u{NAN}; std::string laplaceInterp; double amEarly{NAN}; double amLate{NAN}; };
static TrendResult trend_tests(const std::vector<Record>& recs){
    TrendResult tr{};
    const size_t M = recs.size();
    if(M<2){ tr.laplaceInterp = "INSUFFICIENT_DATA"; return tr; }
    double T = recs.back().failureTime;
    if(T<=0){ tr.laplaceInterp = "INSUFFICIENT_DATA"; return tr; }
    double sumt=0; for(const auto &r:recs) sumt += r.failureTime;
    double u = ( (sumt/M) - (T/2.0) ) / ( T * std::sqrt(1.0/(12.0*M)) );
    tr.laplace_u = u;
    if(u < -2.0) tr.laplaceInterp = "INCREASING"; else if(u>2.0) tr.laplaceInterp = "DECLINING"; else tr.laplaceInterp = "STABLE";
    // Arithmetic mean trend
    size_t k = std::min<size_t>(10, M/2);
    if(k>0){
        double e=0,l=0; for(size_t i=0;i<k;i++) e += (i==0? recs[0].failureTime : recs[i].failureTime - recs[i-1].failureTime);
        for(size_t i=M-k;i<M;i++) l += (i==0? recs[0].failureTime : recs[i].failureTime - recs[i-1].failureTime);
        tr.amEarly = e/k; tr.amLate = l/k;
    }
    return tr;
}

struct FitMetrics { double sse{NAN}; double r2{NAN}; double aic{NAN}; double loglik{NAN}; };
struct ModelResult { std::string name; bool ok{false}; double p1{NAN}, p2{NAN}; // model-specific params
                     double lambda_T{NAN}; double mtbf_T{NAN}; FitMetrics metrics; };

static FitMetrics compute_metrics(const std::vector<Record>& recs, const std::vector<double>& mu_ti, double loglik, int k){
    const size_t M = recs.size();
    FitMetrics m{};
    if(M==0) return m;
    double sse=0; double mean_i = (M+1)/2.0; double sst=0;
    for(size_t i=0;i<M;i++){
        double obs = static_cast<double>(i+1);
        double pred = mu_ti[i];
        sse += (obs - pred)*(obs - pred);
        sst += (obs - mean_i)*(obs - mean_i);
    }
    m.sse = sse;
    m.r2 = (sst>0) ? 1.0 - (sse/sst) : NAN;
    m.loglik = loglik;
    m.aic = 2*k - 2*loglik;
    return m;
}

static ModelResult fit_goel_okumoto(const std::vector<Record>& recs){
    ModelResult r{}; r.name = "GOEL_OKUMOTO";
    const size_t M = recs.size(); if(M<2) return r;
    const double T = recs.back().failureTime; if(T<=0) return r;
    double sumt=0; for(const auto &x:recs) sumt += x.failureTime;
    // 1D optimize b; a*(b)= M*b/(1-exp(-bT))
    double best_b = NAN, best_ll = -INFINITY;
    auto a_of_b = [&](double b){ double denom = 1.0 - std::exp(-b*T); if(denom<=1e-18) denom = 1e-18; return (M*b)/denom; };
    for(double lb=-9; lb<=0; lb+=0.25){ // b in [1e-9, 1]
        double b = std::pow(10.0, lb);
        double a = a_of_b(b);
        // L = M ln a - b sum t_i - (a/b)(1-exp(-bT)) ; the last term equals M by construction
        double ll = M*std::log(a) - b*sumt - M;
        if(ll>best_ll){ best_ll=ll; best_b=b; }
    }
    // small local refine
    for(int step=0; step<25; ++step){
        double b = best_b;
        double scale = std::pow(10.0, -2 - step/25.0); // progressively finer
        double cand[5] = { b*(1-2*scale), b*(1-scale), b, b*(1+scale), b*(1+2*scale) };
        for(double cb : cand){ if(cb<=0) continue; double a=a_of_b(cb); double ll = M*std::log(a) - cb*sumt - M; if(ll>best_ll){best_ll=ll; best_b=cb;} }
    }
    double b = best_b; if(!(b>0)) return r; double a = a_of_b(b);
    // predictions at observed times
    std::vector<double> mu; mu.reserve(M);
    for(size_t i=0;i<M;i++){ double t=recs[i].failureTime; mu.push_back((a/b)*(1.0-std::exp(-b*t))); }
    double lambdaT = a*std::exp(-b*T);
    r.ok = true; r.p1 = a; r.p2 = b; r.lambda_T = lambdaT; r.mtbf_T = (lambdaT>0)? 1.0/lambdaT : INFINITY; r.metrics = compute_metrics(recs, mu, best_ll, 2);
    return r;
}

static ModelResult fit_musa_okumoto(const std::vector<Record>& recs){
    ModelResult r{}; r.name = "MUSA_OKUMOTO";
    const size_t M = recs.size(); if(M<2) return r;
    const double T = recs.back().failureTime; if(T<=0) return r;
    // optimize beta ( = lambda0*theta ), alpha = M / ln(1+beta T)
    double Slog = 0; // used per-beta
    double best_beta=NAN, best_ll=-INFINITY;
    for(double l=-12; l<=4; l+=0.25){ // beta in [1e-12, 1e4]
        double beta = std::pow(10.0,l);
        double ln1p = std::log1p(beta*T);
        if(!(ln1p>0)) continue;
        double alpha = M / ln1p; // 1/theta
        double ll = M*std::log(alpha) + M*std::log(beta);
        double sumln = 0; for(const auto &x:recs){ sumln += std::log1p(beta*x.failureTime); }
        ll -= sumln; ll -= alpha*ln1p; // minus M
        // Note: alpha*ln1p == M, so ll simplifies to M ln alpha + M ln beta - sumln - M
        if(ll>best_ll){ best_ll=ll; best_beta=beta; }
    }
    // local refine
    for(int step=0; step<25; ++step){
        double b = best_beta; if(!(b>0)) break;
        double scale = std::pow(10.0, -2 - step/25.0);
        double cand[5] = { b*(1-2*scale), b*(1-scale), b, b*(1+scale), b*(1+2*scale) };
        for(double cb : cand){ if(!(cb>0)) continue; double ln1p=std::log1p(cb*T); if(!(ln1p>0)) continue; double alpha=M/ln1p; double ll=M*std::log(alpha)+M*std::log(cb); double sumln=0; for(const auto &x:recs){ sumln += std::log1p(cb*x.failureTime);} ll-=sumln; ll-=alpha*ln1p; if(ll>best_ll){best_ll=ll; best_beta=cb;} }
    }
    if(!(best_beta>0)) return r; double beta = best_beta; double alpha = M / std::log1p(beta*T);
    // predictions
    std::vector<double> mu; mu.reserve(M);
    for(size_t i=0;i<M;i++){ double t=recs[i].failureTime; mu.push_back(alpha*std::log1p(beta*t)); }
    double lambdaT = (alpha*beta) / (1.0 + beta*T);
    r.ok = true; r.p1 = alpha; r.p2 = beta; r.lambda_T = lambdaT; r.mtbf_T = (lambdaT>0)? 1.0/lambdaT : INFINITY; r.metrics = compute_metrics(recs, mu, best_ll, 2);
    return r;
}

static ModelResult fit_crow_amsaa(const std::vector<Record>& recs){
    ModelResult r{}; r.name = "CROW_AMSAA";
    const size_t M = recs.size(); if(M<2) return r;
    const double T = recs.back().failureTime; if(T<=0) return r;
    // optimize beta; lambda = M / T^beta
    // Need ln t_i, skip zeros
    std::vector<double> ti; ti.reserve(M); for(const auto &x:recs){ if(x.failureTime>0){ ti.push_back(x.failureTime);} }
    if(ti.size()<2) return r;
    double Sln=0; for(double t:ti) Sln += std::log(t);
    double best_beta=NAN,best_ll=-INFINITY;
    for(double b=0.2; b<=3.0; b+=0.02){
        double ll = M*std::log(M) - M*b*std::log(T) + M*std::log(b) + (b-1.0)*Sln - M; // using mu(T)=M at lambda*; minus M
        if(ll>best_ll){ best_ll=ll; best_beta=b; }
    }
    // small refine around best
    for(int step=0; step<25; ++step){
        double b = best_beta; double delta = 0.1 / (1+step);
        double cand[5] = { b-2*delta, b-delta, b, b+delta, b+2*delta };
        for(double cb : cand){ if(cb<=0) continue; double ll = M*std::log(M) - M*cb*std::log(T) + M*std::log(cb) + (cb-1.0)*Sln - M; if(ll>best_ll){best_ll=ll; best_beta=cb;} }
    }
    if(!(best_beta>0)) return r; double beta = best_beta; double lambda = M/std::pow(T,beta);
    std::vector<double> mu; mu.reserve(M);
    for(size_t i=0;i<M;i++){ double t=recs[i].failureTime; t = std::max(t, 1e-12); mu.push_back(lambda*std::pow(t,beta)); }
    double lambdaT = lambda * beta * std::pow(T, beta-1.0);
    r.ok = true; r.p1 = lambda; r.p2 = beta; r.lambda_T = lambdaT; r.mtbf_T = (lambdaT>0)? 1.0/lambdaT : INFINITY; r.metrics = compute_metrics(recs, mu, best_ll, 2);
    return r;
}

static std::string today_yyyymmdd(){
    std::time_t tt = std::time(nullptr); std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream os; os<<std::put_time(&tm, "%Y%m%d"); return os.str();
}

struct PathWrap {
    std::string s;
    PathWrap()=default;
    explicit PathWrap(const std::string& x):s(x){}
    bool has_parent() const {
        auto pos = s.find_last_of("/\\"); return pos!=std::string::npos; }
    std::string parent() const {
        auto pos = s.find_last_of("/\\"); return (pos==std::string::npos)? std::string(".") : s.substr(0,pos); }
};

static void ensure_dir(const std::string& p){
#if SRG_HAVE_FS
    std::error_code ec; std::filesystem::create_directories(std::filesystem::path(p), ec);
#else
    // Best-effort: create a single level (Windows PowerShell mkdir via system). Avoid complex parsing.
    // Non-critical if fails; report omitted.
#endif
}

int main(int argc, char** argv){
    // Arg1: input export CSV (required). Arg2 (optional): output report path.
    std::string inPath = (argc>1)? argv[1] : std::string("reliability/srg_export.csv");
    PathWrap inP(inPath);
    auto recs = read_csv(inPath);
    const size_t M = recs.size();
    double T = (M>0)? recs.back().failureTime : 0.0;
    auto trend = trend_tests(recs);

    // Fit models
    std::vector<ModelResult> models;
    models.push_back(fit_musa_okumoto(recs));
    models.push_back(fit_goel_okumoto(recs));
    models.push_back(fit_crow_amsaa(recs));

    // Pick best by AIC among ok models
    int bestIdx = -1; double bestAIC = std::numeric_limits<double>::infinity();
    for(size_t i=0;i<models.size();++i){ if(models[i].ok && models[i].metrics.aic < bestAIC){ bestAIC = models[i].metrics.aic; bestIdx = static_cast<int>(i);} }

    // Determine output report path
    std::string outPath;
    if(argc>2){ outPath = argv[2]; }
    else {
        // Assume input path ends with reliability/filename.csv. Go two levels up if possible.
        std::string baseParent = inP.parent(); // reliability
        PathWrap parentWrap(baseParent);
        std::string root = parentWrap.parent();
        outPath = root + "/verification/srg-analysis-main-" + today_yyyymmdd() + ".md";
    }
    // Ensure directory exists
    // Extract parent
    PathWrap outWrap(outPath);
    ensure_dir(outWrap.parent());

    // Write report
    std::ofstream md(outPath.c_str());
    md << "# SRG Analysis Report\n\n";
    md << "Dataset: " << inPath << "\n\n";
    md << "- Records: " << M << "\n";
    md << std::fixed << std::setprecision(6);
    md << "- Total time T: " << T << "\n";
    md << "- Laplace u: " << (std::isnan(trend.laplace_u)? 0.0 : trend.laplace_u) << " (" << trend.laplaceInterp << ")\n";
    if(!std::isnan(trend.amEarly) && !std::isnan(trend.amLate)){
        md << "- AM early vs late TBF: " << trend.amEarly << " -> " << trend.amLate << "\n";
    }
    md << "\n## Models\n\n";
    for(const auto &m: models){
        md << "### " << m.name << "\n\n";
        if(!m.ok){ md << "Status: insufficient data or invalid inputs.\n\n"; continue; }
        md << "Parameters: p1=" << m.p1 << ", p2=" << m.p2 << "\n";
        md << "lambda(T)=" << m.lambda_T << ", MTBF(T)=" << m.mtbf_T << "\n";
        md << "SSE=" << m.metrics.sse << ", R2=" << m.metrics.r2 << ", AIC=" << m.metrics.aic << "\n\n";
    }
    if(bestIdx>=0){ md << "Best model (AIC): " << models[bestIdx].name << "\n"; }
    else { md << "Best model (AIC): none (insufficient data)\n"; }
    md.close();

    // Console summary for CTest
    int okCount = 0; for(const auto &m:models) if(m.ok) okCount++;
    std::cout << "SRG_FIT: records=" << M
              << ", T=" << T
              << ", models_ok=" << okCount
              << ", trend=" << trend.laplaceInterp
              << ", report=\"" << outPath << "\"\n";
    return 0;
}
