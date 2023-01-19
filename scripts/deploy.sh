#!/bin/bash

# First define which applications and versions to build, see the end of this script
# This script needs to be called from chester/applications folder by typing ../scripts/deploy.sh

pause() {
    read -rsp $'Press enter to continue...\n'
}

deploy_clime() {

    local FW_VERSION="$1"

    cd clime

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_lte ctr_s2)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Clime
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime" --version $FW_VERSION

    #
    # Build CHESTER Clime Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lte ctr_s2)*/, "set(SHIELD ctr_lte ctr_s2 ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-z" --version $FW_VERSION

    #
    # Build CHESTER Clime IAQ
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lte ctr_s2)*/, "set(SHIELD ctr_lte ctr_s1)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime IAQ" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-iaq" --version $FW_VERSION

    #
    # Build CHESTER Clime 1W
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lte ctr_s2)*/, "set(SHIELD ctr_lte ctr_ds18b20)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime 1W" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-1w" --version $FW_VERSION

    #
    # Build CHESTER Clime RTD
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lte ctr_s2)*/, "set(SHIELD ctr_lte ctr_rtd_a)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime RTD" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-rtd" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
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
    hardwario chester app fw upload --name "hio-chester-current" --version $FW_VERSION

    #
    # Build CHESTER Current Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_k1 ctr_lte)*/, "set(SHIELD ctr_k1 ctr_lte ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-current-z" --version $FW_VERSION

    #
    # Build CHESTER Current 1W
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_k1 ctr_lte)*/, "set(SHIELD ctr_ds18b20 ctr_k1 ctr_lte)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current 1W" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-current-1w" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

#deploy_current "v0.0.1"
#deploy_clime "v1.9.0"
