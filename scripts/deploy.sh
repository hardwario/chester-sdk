#!/bin/bash

#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

# First define which applications and versions to build, see the end of this script
# This script needs to be called from chester/applications folder by typing ../scripts/deploy.sh

pause() {
    read -rsp $'Press enter to continue...\n'
}

deploy_clime() {

    local FW_VERSION="$1"

    cd clime

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_lrw ctr_lte ctr_s2)" "CMakeLists.txt"; then
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
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lrw ctr_lte ctr_s2)*/, "set(SHIELD ctr_lrw ctr_lte ctr_s2 ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-z" --version $FW_VERSION

    #
    # Build CHESTER Clime IAQ
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lrw ctr_lte ctr_s2)*/, "set(SHIELD ctr_lrw ctr_lte ctr_s1)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime IAQ" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-iaq" --version $FW_VERSION

    #
    # Build CHESTER Clime 1W
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lrw ctr_lte ctr_s2)*/, "set(SHIELD ctr_lrw ctr_lte ctr_ds18b20)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime 1W" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-1w" --version $FW_VERSION

    #
    # Build CHESTER Clime 1WH
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lrw ctr_lte ctr_s2)*/, "set(SHIELD ctr_lrw ctr_lte ctr_ds18b20 ctr_s2)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime 1WH" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-1wh" --version $FW_VERSION

    #
    # Build CHESTER Clime RTD
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lrw ctr_lte ctr_s2)*/, "set(SHIELD ctr_lrw ctr_lte ctr_rtd_a)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Clime RTD" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-clime-rtd" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}


deploy_push() {

    local FW_VERSION="$1"

    cd push

    # Check default prj.conf we expect
    if grep -q "CONFIG_APP_FLIP_MODE=n" "prj.conf"; then
        echo "String found ok"
    else
        echo "Default CONFIG_APP_FLIP_MODE=n string not found, update deploy script."
        exit
    fi

    # Backup original prj.conf
    cp prj.conf prj.conf.bak

    #
    # Build CHESTER Push
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Push" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-push" --version $FW_VERSION

    #
    # Build CHESTER Push FM
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/CONFIG_APP_FLIP_MODE=n*/, "CONFIG_APP_FLIP_MODE=y"); print }' prj.conf
    rm -rf build/
    FW_NAME="CHESTER Push FM" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-push-fm" --version $FW_VERSION

    # Recover original prj.conf
    cp prj.conf.bak prj.conf
    rm prj.conf.bak

    cd ..
}

deploy_counter() {

    local FW_VERSION="$1"

    cd counter

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_lte ctr_x0_a)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Counter
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Counter" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-counter" --version $FW_VERSION

    #
    # Build CHESTER Counter Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lte ctr_x0_a)*/, "set(SHIELD ctr_lte ctr_x0_a ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Counter Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-counter-z" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

deploy_input() {

    local FW_VERSION="$1"

    cd input

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_ds18b20 ctr_lte ctr_x0_a ctr_z)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Input
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    # Default CMakeLists contains ctr_z, so we remove it
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_ds18b20 ctr_lte ctr_x0_a ctr_z)*/, "set(SHIELD ctr_ds18b20 ctr_lte ctr_x0_a)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Input" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-input" --version $FW_VERSION

    #
    # Build CHESTER Input Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    # Default CMakeLists contains ctr_z, so we do not do any string replace
    rm -rf build/
    FW_NAME="CHESTER Input Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-input-z" --version $FW_VERSION

    #
    # Build CHESTER Input ZH
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_ds18b20 ctr_lte ctr_x0_a ctr_z)*/, "set(SHIELD ctr_ds18b20 ctr_lte ctr_x0_a ctr_z ctr_s2)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Input ZH" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-input-zh" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

deploy_current() {

    local FW_VERSION="$1"

    cd current

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_k1 ctr_lrw ctr_lte)" "CMakeLists.txt"; then
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
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_k1 ctr_lrw ctr_lte)*/, "set(SHIELD ctr_k1 ctr_lrw ctr_lte ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-current-z" --version $FW_VERSION

    #
    # Build CHESTER Current 1W
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_k1 ctr_lrw ctr_lte)*/, "set(SHIELD ctr_ds18b20 ctr_k1 ctr_lrw ctr_lte)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Current 1W" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-current-1w" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

deploy_scale() {

    local FW_VERSION="$1"

    cd scale

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_lte ctr_x3_a)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Scale
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Scale" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-scale" --version $FW_VERSION

    #
    # Build CHESTER Scale Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_lte ctr_x3_a)*/, "set(SHIELD ctr_lte ctr_x3_a ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Scale Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-scale-z" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

deploy_meteo() {

    local FW_VERSION="$1"

    cd meteo

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_barometer_tag ctr_ds18b20 ctr_lte ctr_s2 ctr_meteo_a ctr_soil_sensor)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Meteo
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Meteo" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-meteo" --version $FW_VERSION

    #
    # Build CHESTER Meteo Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_barometer_tag ctr_ds18b20 ctr_lte ctr_s2 ctr_meteo_a ctr_soil_sensor)*/, "set(SHIELD ctr_barometer_tag ctr_ds18b20 ctr_lte ctr_s2 ctr_meteo_a ctr_soil_sensor ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Meteo Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-meteo-z" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

deploy_range() {

    local FW_VERSION="$1"

    cd range

    # Check default SHIELDs we expect
    if grep -q "set(SHIELD ctr_ds18b20 ctr_lte ctr_mb7066_a)" "CMakeLists.txt"; then
        echo "String found ok"
    else
        echo "Default SHIELD string not found, update deploy script."
        exit
    fi

    # Backup original CMakeLists
    cp CMakeLists.txt CMakeLists.txt.bak

    #
    # Build CHESTER Range
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Range" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-range" --version $FW_VERSION

    #
    # Build CHESTER Range Z
    #
    cp CMakeLists.txt.bak CMakeLists.txt
    gawk -i inplace '{ gsub(/set\(SHIELD ctr_ds18b20 ctr_lte ctr_mb7066_a)*/, "set(SHIELD ctr_ds18b20 ctr_lte ctr_mb7066_a ctr_z)"); print }' CMakeLists.txt
    rm -rf build/
    FW_NAME="CHESTER Range Z" FW_VERSION=$FW_VERSION west build
    hardwario chester app fw upload --name "hio-chester-range-z" --version $FW_VERSION

    # Recover original CMakeLists
    cp CMakeLists.txt.bak CMakeLists.txt
    rm CMakeLists.txt.bak

    cd ..
}

# deploy_clime "v2.3.0"
# deploy_push "v2.3.0"
# deploy_current "v2.3.0"
# deploy_counter "v2.3.0"
# deploy_input "v2.3.0"
# deploy_scale "v2.3.0"
# deploy_meteo "v2.3.0"
# deploy_range "v2.3.0"
