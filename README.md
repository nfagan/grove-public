# grove

This is the public repository for grove, a sandbox video game about sound, the environment, and interactive dynamic systems.

This document is intended as a resource for people looking to build and run the program from source, and / or for developers interested in modifying or extending the program. For a non-technical introduction to this work, see the accompanying website: [https://grove.ooo](https://grove.ooo).

# build

The simplest way to build and run this program -- and what I would recommend to most people -- follows. A more complete and technical build-guide is forthcoming.

## build for macos

**grove has only been tested on and only officially supports Apple-silicon (e.g. M1) based Macs.** If you have an Intel-based Mac and would consider attempting the following steps, I would be grateful to hear your experience. I am reachable here (you can file an issue) or via email: fagan dot nicholas at gmail.

* Press command and space to open spotlight search; search for `terminal.app` and press enter to open a terminal window.
* Type `git --version` and press enter. This checks to see if git is installed. If it is, a version string will be printed (e.g. git version 2.32.0). Otherwise, you will be prompted to install developer tools from Apple; click accept/ok to install these tools.
* [Download and install](https://code.visualstudio.com) Visual Studio Code if you do not already have it.
* Near the top of this page, click the green button labeled `<> Code` and select Download ZIP. This will download the repository, usually to your Downloads folder. Unzip this file by double-clicking it. You can also clone the repository via the terminal if you would prefer.
* [Download and install](https://sdk.lunarg.com/sdk/download/1.3.236.0/mac/vulkansdk-macos-1.3.236.0.dmg) the Vulkan SDK. **Important note**: This link points to the specific SDK version (1.3.236.0) I have validated with my M1-based machine. Newer SDK versions might work, but have not been tested. When selecting components to install, the defaults (you can just click continue through the installer).
* 