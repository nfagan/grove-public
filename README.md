# grove

This is the public repository for grove, a sandbox video game about sound, the environment, and interactive dynamic systems.

This document is intended as a resource for people looking to build and run the program from source, and / or for developers interested in modifying or extending the program. For a non-technical introduction to this work, see the accompanying website: [https://grove.ooo](https://grove.ooo).

# build

The simplest way to build and run this program -- and what I would recommend to most people -- follows. A more complete and technical build-guide is forthcoming.

## build for macos

**grove has only been tested on and only officially supports Apple-silicon (e.g. M1) based Macs.** If you have an Intel-based Mac and would consider attempting the following steps, I would be grateful to hear your experience. I am reachable here (you can file an issue) or via email: fagan dot nicholas at gmail.

* Press command and space to open spotlight search; search for `terminal.app` and press enter to open a terminal window.
* Type `git --version` and press enter. This checks to see if git is installed. If it is, a version string will be printed (e.g. git version 2.32.0). Otherwise, you will be prompted to install developer tools from Apple; click accept/ok to install these tools.
* Clone (download) this repository using git (copy and paste the following into a terminal window):
```bash
cd ~/Downloads
git clone --recursive https://github.com/nfagan/grove-public
```
* [Download and install](https://code.visualstudio.com) Visual Studio Code if you do not already have it.
* [Download and install](https://sdk.lunarg.com/sdk/download/1.3.236.0/mac/vulkansdk-macos-1.3.236.0.dmg) the Vulkan SDK. **Important note**: This link points to the specific SDK version (1.3.236.0) I have validated with my M1-based machine. Newer SDK versions might work, but have not been tested. When selecting components to install, the defaults are fine (you can just click continue through the installer).
* Open Visual Studio Code and, from the menu bar, select File -> Open Folder. Navigate to your Downloads folder, locate the folder `grove-public` and click Open.
* If not already installed, Visual Studio Code will ask if you would like to install a recommended extension for C/C++ development from Microsoft. Press OK to install this extension.
* Once installed, you will be prompted to select a "kit". This is the compiler that will be used to build the program. You may have multiple on your system; select one that begins with "clang".
* 