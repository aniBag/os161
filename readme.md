# OS/161
# 
# For the processor, memory, and disks, we will discuss how the 
# operating system allocates each resource and explore the 
# design and implementation of related abstractions. We will also 
# establish techniques for testing and improving system 
# performance and introduce the idea of hardware virtualization.


# PREREQUISITES
# 
# Ubuntu 16.04 / 14.04  - Headless or with GUI
#                       - Native OS / Virtual Machine
# 
# Git - Install Git (Ubuntu) 
#             - sudo apt-get install git
# 
#     - Configure Git 
#             - git config --global user.email "example@example.com"
#             - git config --global user.name "username"


# SETUP
# 
# Installing Toolchain - Installing the OS161 toolchain
# 
#                      - Installation 
#                              - sudo add-apt-repository ppa:ops-class/os161-toolchain
#                              - sudo apt-get update
#                              - sudo apt-get install os161-toolchain
# 			       - sudo apt-get update
#                              - sudo apt-get install os161-toolchain
# 
# Source Files - Clone the OS161 Source files from Github.
# 
#              - Execute the given command in the desired directory.
#                      - git clone https://github.com/aniBag/os161.git
# 
# Compiled Files - In the /os161 directory execute the following command.
#                	- ./configure --ostree=PATH
#               		- Replace PATH with the desired path.
#                		- Ex: --ostree=$HOME/root
#                		- Make sure the root folder is not in the OS161 directory
#
# Configurating the OS161 - Navigate to the directory os161/kern/conf and execute:
#                                 - ./config DUMBVM
#                                 	- This will compile the kernel according to DUMBVM configuration.
#                                 	- This will create ../compile/DUMBVM directory.
# 
#                         - Navigate to /os161/kern/compile/DUMBVM and execute the following:
#                                 - bmake depend
#                                 - bmake
#                                 - bmake install
#                                         - These commands will compile the kernel in the /root directory.
# 
#                         - Download the sys161.conf file from https://www.ops-class.org/files/sys161.conf and place it in the /root directory.
# 
# Booting the OS - After performing the above steps the OS is ready to boot.
#                - In the /root directory execute:
#                        - sys161 kernel
#                                - This will boot your kernel.
# 
# Installing the User Tools - In /os161 execute the following commands:
#                                   - bmake -j8
#                                   - bmake -j8 install
#                                           - These commands will install the User Tools in the /root directory.
