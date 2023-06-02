# grove

This is the public repository for grove, a sandbox video game about sound, the environment, and interactive dynamic systems.

This document is intended as a resource for people looking to build and run the program from source, and / or for developers interested in modifying or extending the program. For a non-technical introduction to this work, see the accompanying website: [https://grove.ooo](https://grove.ooo).

# build

The simplest way to build and run this program -- and what I would recommend to most people -- follows. A more complete and technical build-guide is forthcoming.

## build for macos

**grove has only been tested on and only officially supports Apple-silicon (e.g. M1) based Macs.** If you have an Intel-based Mac and would consider attempting the following steps, I would be grateful to hear your experience. I am reachable here (you can file an issue) or via email: fagan dot nicholas at gmail.

### prerequesites

* `git` and Apple's developer tools are required. 
 Press command and space to open spotlight search; search for `terminal.app` and press enter to open a terminal window. Type `git --version` and press enter. This checks to see if git is installed. If it is, a version string will be printed (e.g. git version 2.32.0). Otherwise, you will be prompted to install Apples' developer tools; click accept/ok to install these tools.
* [Download and install](https://sdk.lunarg.com/sdk/download/1.3.236.0/mac/vulkansdk-macos-1.3.236.0.dmg) the Vulkan SDK. **Important note**: This link points to the specific SDK version (1.3.236.0) I have validated with my M1-based machine. Newer SDK versions might work, but have not been tested. When selecting components to install, the defaults are fine (you can just click continue through the installer).
* [Download and install](https://cmake.org/download/) cmake. If given the option to add cmake to the path, click OK to enable this.

### build

* Open a new terminal window (command + space, search for terminal). Press command + t to open a new terminal tab, in case you were using the same terminal window from above. The following commands can be copied and pasted into this window.
* Clone (download) this repository using git:
```bash
cd ~/Downloads
git clone --recursive https://github.com/nfagan/grove-public
```
* Run the following to build the program:
```bash
cd ~/Downloads/grove-public && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 ..
cmake --build . --target vk_app --config Release -- -j 8
```
* Run the following to run the program:
```bash
cd ~/Downloads/grove-public
./build/src/vk-app/vk_app -nt 1 -rd ./assets
```