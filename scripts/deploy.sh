#!/bin/bash

# First define which applications and versions to build, see the end of this script
# This script needs to be called from chester/applications folder by typing ../scripts/deploy.sh

pause() {
    read -rsp $'Press enter to continue...\n'
}

deploy_current() {

    local FW_VERSION="$1"

    cd current

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_k1 ctr_lte)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Current
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "CHESTER Current" --version $FW_VERSION

    #
    # Build CHESTER Current Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_k1 ctr_lte)*/, "set(SHIELD ctr_k1 ctr_lte ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "CHESTER Current Z" --version $FW_VERSION

    #
    # Build CHESTER Current 1W
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_k1 ctr_lte)*/, "set(SHIELD ctr_ds18b20 ctr_k1 ctr_lte)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current 1W" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "CHESTER Current 1W" --version $FW_VERSION

    pause

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

#deploy_current "v0.0.1"
