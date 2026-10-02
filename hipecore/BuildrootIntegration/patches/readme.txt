These patches were taken from the buildroot-2024.11.1 packaged version of QtWebKit.
They fix build problems on certain systems.

This folder may be deleted if desired, as the patches (except for the ARC CPU support patch
which is not required) have already been applied to the code files.
This folder is kept for reference only.

0007 (Offlineasm / Ruby 2.7) was dropped: it patched Source/JavaScriptCore/offlineasm,
which no longer exists in this fork (the JavaScript engine has been removed).
