/**
 * @file ptp_config_via_oid.cpp
 * @brief Configure Intel I226 PTP via Windows OID requests to main network driver
 * 
 * This approach bypasses the AVB Filter Driver and directly configures
 * the network adapter through Windows NDIS OID requests.
 * 
 * Requires: Administrator privileges, WinPcap/Npcap driver
 */

#include <windows.h>
#include <iostream>
#include <iomanip>
#include <vector>

// Windows Network Driver OIDs
#define OID_GEN_VENDOR_DESCRIPTION          0x0001010D
#define OID_GEN_VENDOR_ID                   0x0001010C
#define OID_GEN_HARDWARE_STATUS             0x00010102

// Intel-specific OIDs for PTP (custom - may not be documented)
#define OID_INTEL_PTP_ENABLE                0xFF000001
#define OID_INTEL_PTP_SET_TIME              0xFF000002
#define OID_INTEL_PTP_GET_TIME              0xFF000003

/**
 * @brief Send OID request to network adapter
 */
bool send_oid_request(const wchar_t* adapter_name, DWORD oid, void* buffer, DWORD buffer_size, bool set_request) {
    // TODO: Implementation requires either:
    // 1. WinPcap/Npcap PacketRequest API
    // 2. Direct NdisRequest via custom driver
    // 3. DeviceIoControl to \Device\{GUID} with METHOD_BUFFERED
    
    std::wcout << L"OID Request: " << oid << L" to " << adapter_name << std::endl;
    std::wcout << L"This functionality requires:\n";
    std::wcout << L"  1. WinPcap/Npcap for PacketRequest(), OR\n";
    std::wcout << L"  2. Custom NDIS miniport filter driver, OR\n";
    std::wcout << L"  3. Intel ProSet SDK (if available)\n";
    
    return false;
}

/**
 * @brief Alternative: Suggest using Intel ProSet utilities
 */
void suggest_intel_proset_approach() {
    std::cout << "\n========================================\n";
    std::cout << "Alternative PTP Configuration Methods\n";
    std::cout << "========================================\n\n";
    
    std::cout << "Method 1: Intel ProSet Command Line Tools\n";
    std::cout << "  If Intel Ethernet Adapter Management is installed:\n";
    std::cout << "  > prosetcl.exe /HELP\n";
    std::cout << "  > prosetcl.exe /SET_PTP_ENABLE=1\n\n";
    
    std::cout << "Method 2: Intel ANS (Advanced Network Services)\n";
    std::cout << "  Intel ANS configuration files in:\n";
    std::cout << "  C:\\Program Files\\Intel\\ANS\\config\\\n\n";
    
    std::cout << "Method 3: Registry Configuration\n";
    std::cout << "  HKLM\\SYSTEM\\CurrentControlSet\\Control\\Class\\\n";
    std::cout << "  {4d36e972-e325-11ce-bfc1-08002be10318}\\<Instance>\n";
    std::cout << "  Look for PTP-related registry keys\n\n";
    
    std::cout << "Method 4: Directly Enable via Device Manager\n";
    std::cout << "  1. Open Device Manager\n";
    std::cout << "  2. Network Adapters -> Intel I226\n";
    std::cout << "  3. Properties -> Advanced Tab\n";
    std::cout << "  4. Look for 'PTP Hardware Timestamp' or similar\n";
    std::cout << "  5. Enable if available\n\n";
    
    std::cout << "Method 5: Use PowerShell NetAdapter Cmdlets\n";
    std::cout << "  Get-NetAdapter | Where-Object {$_.DriverDescription -like '*I226*'}\n";
    std::cout << "  Get-NetAdapterAdvancedProperty -Name 'Ethernet 3'\n";
    std::cout << "  Set-NetAdapterAdvancedProperty -Name 'Ethernet 3' -RegistryKeyword '*PTP*' -RegistryValue 1\n\n";
}

/**
 * @brief Check registry for PTP settings
 */
void check_registry_ptp_settings() {
    std::cout << "\n=== Checking Registry for PTP Settings ===\n";
    
    HKEY hKey;
    const wchar_t* reg_path = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}";
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, reg_path, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        std::cout << "Network Adapters registry key opened successfully\n";
        std::cout << "Enumerating subkeys for Intel I226 adapters...\n\n";
        
        DWORD index = 0;
        wchar_t subkey_name[256];
        DWORD subkey_size = sizeof(subkey_name) / sizeof(wchar_t);
        
        while (RegEnumKeyExW(hKey, index, subkey_name, &subkey_size, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            // Open each subkey
            HKEY hSubKey;
            if (RegOpenKeyExW(hKey, subkey_name, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                // Check DriverDesc
                wchar_t driver_desc[256] = {0};
                DWORD desc_size = sizeof(driver_desc);
                DWORD desc_type = REG_SZ;
                
                if (RegQueryValueExW(hSubKey, L"DriverDesc", nullptr, &desc_type, (LPBYTE)driver_desc, &desc_size) == ERROR_SUCCESS) {
                    if (wcsstr(driver_desc, L"I226") != nullptr) {
                        std::wcout << L"Found: " << subkey_name << L" - " << driver_desc << std::endl;
                        
                        // Look for PTP-related keys
                        wchar_t value_name[256];
                        DWORD value_name_size;
                        DWORD value_index = 0;
                        
                        while (true) {
                            value_name_size = sizeof(value_name) / sizeof(wchar_t);
                            DWORD value_type;
                            DWORD value_data;
                            DWORD value_data_size = sizeof(value_data);
                            
                            LONG result = RegEnumValueW(hSubKey, value_index, value_name, &value_name_size, 
                                                       nullptr, &value_type, (LPBYTE)&value_data, &value_data_size);
                            if (result != ERROR_SUCCESS) break;
                            
                            // Check for PTP, Timestamp, 1588 related keys
                            if (wcsstr(value_name, L"PTP") || wcsstr(value_name, L"Timestamp") || 
                                wcsstr(value_name, L"1588") || wcsstr(value_name, L"Time")) {
                                std::wcout << L"  " << value_name << L" = ";
                                if (value_type == REG_DWORD) {
                                    std::wcout << value_data;
                                }
                                std::wcout << std::endl;
                            }
                            
                            value_index++;
                        }
                    }
                }
                
                RegCloseKey(hSubKey);
            }
            
            index++;
            subkey_size = sizeof(subkey_name) / sizeof(wchar_t);
        }
        
        RegCloseKey(hKey);
    } else {
        std::cerr << "Failed to open network adapters registry key\n";
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Intel I226 PTP Configuration via OID\n";
    std::cout << "========================================\n\n";
    
    std::cout << "NOTE: The Intel AVB Filter Driver provides read-only access\n";
    std::cout << "      to PTP registers. Configuration must be done through\n";
    std::cout << "      the main Intel network driver.\n\n";
    
    // Check registry for existing PTP configuration
    check_registry_ptp_settings();
    
    // Show alternative configuration methods
    suggest_intel_proset_approach();
    
    std::cout << "\n=== Conclusion ===\n";
    std::cout << "The Intel AVB Filter Driver is designed for:\n";
    std::cout << "  ✓ Monitoring PTP timestamps (read-only)\n";
    std::cout << "  ✓ Capturing TX/RX timestamp events\n";
    std::cout << "  ✓ TSN feature monitoring\n\n";
    
    std::cout << "PTP Hardware Configuration requires:\n";
    std::cout << "  → Intel main network driver (e1000e, igc, or intel_ethernet)\n";
    std::cout << "  → Windows Network Driver Interface (NDIS)\n";
    std::cout << "  → Registry settings or Intel ProSet utilities\n\n";
    
    std::cout << "Recommendation:\n";
    std::cout << "  1. Enable PTP in Device Manager Advanced Properties\n";
    std::cout << "  2. Use AVB Filter Driver for timestamp reading ONLY\n";
    std::cout << "  3. Clock initialization happens automatically on driver load\n\n";
    
    std::cout << "Press Enter to exit...";
    std::cin.get();
    
    return 0;
}
