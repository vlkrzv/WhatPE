# WhatPE — build metadata for EXE/DLL files, right in the Properties dialog

Windows Explorer's Properties dialog shows Size, Created/Modified/Accessed timestamps, and other
generic file attributes — but nothing about how the binary was actually *built*. WhatPE adds a
**"PE Info" tab** to the standard Properties dialog for EXE/DLL files, so the answer to "what PE is
this?" is one right-click away:

Right-click a .exe or .dll → Properties → **PE Info**:

<img src="screenshots/dotnet_exe.png" height="600" alt=".NET binary — PE Info tab">
<img src="screenshots/native_exe.png" height="600" alt="Native Windows binary — PE Info tab">

* Actual build timestamp (the file's Created/Modified date is often wrong — copies, extracts, and
  CI artifacts reset it)
* Build configuration (Release or Debug)
* Bitness (32-bit, 64-bit, or AnyCPU)
* Compiler / Visual Studio toolset version
* Target .NET Framework version and assembly version (for .NET binaries)
* Security mitigations baked into the binary: DEP, ASLR, CFG

This information is technically all present in the PE header, but reading it normally means
opening the file in a full-blown PE viewer and digging through raw header fields. WhatPE decodes it
and puts it exactly where you already look for file info — no separate tool to open, no fields to
decode by hand.
