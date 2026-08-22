#pragma once

// CLSID of the "What PE?" IExplorerCommand coclass, declared in the package manifest's
// com:Extension (windows.comServer) and referenced by the rightClickContextMenu app extension.
// Not registry-registered anywhere - activation is resolved purely via the MSIX package manifest.
class __declspec(uuid("EB945291-A13D-497A-9CF3-C90986E0C8A1")) WhatPEExplorerCommandClass;
