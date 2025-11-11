/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "IEEE 1588-2019 PTP Library", "index.html", [
    [ "IEEE 1588-2019 – Precision Time Protocol (PTPv2) Core Library", "index.html#autotoc_md0", [
      [ "</blockquote>", "index.html#autotoc_md2", null ],
      [ "Table of Contents", "index.html#autotoc_md3", null ],
      [ "Purpose & Scope", "index.html#autotoc_md5", null ],
      [ "Design Principles", "index.html#autotoc_md6", null ],
      [ "Architecture Overview", "index.html#autotoc_md7", null ],
      [ "Feature Summary", "index.html#autotoc_md8", null ],
      [ "Repository Layout", "index.html#autotoc_md9", null ],
      [ "Getting Started", "index.html#autotoc_md10", [
        [ "Prerequisites", "index.html#autotoc_md11", null ],
        [ "Quick Start", "index.html#autotoc_md12", null ]
      ] ],
      [ "Building", "index.html#autotoc_md13", null ],
      [ "Usage Examples", "index.html#autotoc_md14", null ],
      [ "Testing & Traceability", "index.html#autotoc_md15", null ],
      [ "Roadmap", "index.html#autotoc_md16", null ],
      [ "Contributing", "index.html#autotoc_md17", null ],
      [ "Compliance & References", "index.html#autotoc_md18", null ],
      [ "FAQ", "index.html#autotoc_md19", null ],
      [ "License", "index.html#autotoc_md20", null ],
      [ "Last Updated", "index.html#autotoc_md21", null ]
    ] ],
    [ "IEEE 1588-2019 PTP Library - API Reference Guide", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html", [
      [ "Table of Contents", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md24", null ],
      [ "1. Introduction", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md26", [
        [ "1.1 Overview", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md27", null ],
        [ "1.2 Namespace Structure", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md28", null ],
        [ "1.3 Design Principles", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md29", null ]
      ] ],
      [ "2. Core Types and Constants", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md31", [
        [ "2.1 Basic Integer Types", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md32", null ],
        [ "2.2 PTP-Specific Types", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md33", [
          [ "ClockIdentity", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md34", null ],
          [ "PortNumber", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md35", null ],
          [ "DomainNumber", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md36", null ]
        ] ],
        [ "2.3 Timestamp", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md37", null ],
        [ "2.4 TimeInterval", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md38", null ],
        [ "2.5 CorrectionField", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md39", null ],
        [ "2.6 Port States", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md40", null ],
        [ "2.7 Clock Types", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md41", null ]
      ] ],
      [ "3. Clock State Machines", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md43", [
        [ "3.1 Ordinary Clock", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md44", null ],
        [ "3.2 PtpPort", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md45", null ],
        [ "3.3 Boundary Clock", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md46", null ],
        [ "3.4 Transparent Clock", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md47", null ]
      ] ],
      [ "4. Message Handling", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md49", [
        [ "4.1 Message Types", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md50", null ],
        [ "4.2 Common Message Header", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md51", null ],
        [ "4.3 Announce Message", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md52", null ],
        [ "4.4 Sync Message", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md53", null ],
        [ "4.5 Delay_Req and Delay_Resp Messages", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md54", null ]
      ] ],
      [ "5. Data Sets", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md56", [
        [ "5.1 Default Data Set", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md57", null ],
        [ "5.2 Current Data Set", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md58", null ],
        [ "5.3 Parent Data Set", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md59", null ],
        [ "5.4 Time Properties Data Set", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md60", null ],
        [ "5.5 Port Data Set", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md61", null ]
      ] ],
      [ "6. Hardware Abstraction Layer (HAL)", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md63", [
        [ "6.1 State Callbacks", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md64", null ],
        [ "6.2 HAL Implementation Example", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md65", null ]
      ] ],
      [ "7. Best Master Clock Algorithm (BMCA)", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md67", [
        [ "7.1 BMCA Decision", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md68", null ],
        [ "7.2 BMCA Comparison Steps", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md69", null ]
      ] ],
      [ "8. Error Handling", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md71", [
        [ "8.1 PTPError Enum", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md72", null ],
        [ "8.2 PTPResult Template", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md73", null ]
      ] ],
      [ "9. Usage Examples", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md75", [
        [ "9.1 Basic Slave Clock", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md76", null ],
        [ "9.2 Master Clock", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md77", null ]
      ] ],
      [ "10. Platform Integration", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md79", [
        [ "10.1 CMake Integration", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md80", null ],
        [ "10.2 FetchContent Integration", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md81", null ],
        [ "10.3 Build Options", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md82", null ]
      ] ],
      [ "Appendix A: Quick Reference", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md84", [
        [ "Common Operations", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md85", null ],
        [ "Common Pitfalls", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md86", null ]
      ] ],
      [ "Appendix B: IEEE 1588-2019 Section References", "db/d79/md__2home_2runner_2work_2IEEE__1588__2019_2IEEE__1588__2019_208-transition_2user-documentation_2api-reference.html#autotoc_md88", null ]
    ] ],
    [ "Requirements", "d0/d9d/requirements.html", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Enumerations", "functions_enum.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"d2/d59/namespaceCommon_1_1utils_1_1logging.html#ad05efa82ed52c3f396b417b74bea3aa0",
"d4/d64/messages_8hpp.html#a5b63f41ca533a3a7fa2dbec9e5aa69b9",
"d5/d1f/classIEEE_1_1__1588_1_1PTP_1_1__2019_1_1Clocks_1_1TransparentClock.html#ae5b595941d716388f3039dec2c51fe6d",
"d8/d7c/namespaceIEEE_1_1__1588_1_1PTP_1_1__2019_1_1TLVType.html#ab5aedb8faa2bd83b97774c43adb17326",
"dc/d50/structIEEE_1_1__1588_1_1PTP_1_1__2019_1_1Integration_1_1BMCAStatistics.html#a06add222e1323fa661a3e90d97135eb6",
"dd/d8c/structIEEE_1_1__1588_1_1PTP_1_1__2019_1_1AnnounceBody.html#a125a1ce2ae70a3ac7310d6e48d52a9a4",
"de/d06/namespaceIEEE_1_1__1588_1_1PTP_1_1__2019_1_1ManagementId.html#af772e2adf5124313d464484b7dd66636",
"functions_func_r.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';