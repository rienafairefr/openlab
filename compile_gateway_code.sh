#! /bin/bash

set -e

compile_release()
{
    local platform=$1
    local build=build.release.${platform}

    mkdir -p ${build}
    cd ${build}
    cmake .. -DPLATFORM=${platform} -DRELEASE=2
    make
    cd ..
}

gateway_fw_path()
{
    local platform=$1
    local build=build.release.${platform}
    ls ${build}/bin/*autotest.elf
    ls ${build}/bin/*idle.elf
}

tutorial_fw_path()
{
    local platform=$1
    local build=build.release.${platform}
    ls ${build}/bin/tutorial_*.elf
}

cn_fw_path()
{
    local platform=$1
    local build=build.release.${platform}
    ls ${build}/bin/control_node.elf
}


compile_release iotlab-cn
compile_release iotlab-m3
compile_release iotlab-a8-m3
compile_release agile-fox

cn_fw_path      iotlab-cn
gateway_fw_path iotlab-m3
gateway_fw_path iotlab-a8-m3
gateway_fw_path agile-fox
tutorial_fw_path iotlab-m3
tutorial_fw_path iotlab-a8-m3
