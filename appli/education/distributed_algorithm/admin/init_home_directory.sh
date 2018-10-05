#! /bin/bash


readonly SITE="grenoble.iot-lab.info"

readonly ACCOUNTS="miscit"
readonly NUM="10"


run_ssh()
{
    local login=$1
    local cmd="$2"

    ssh -p 22 ${login}@${SITE} "${cmd}"
}

create_distributed_algorithm_link()
{
    local login=$1
    local cmd1='git clone https://github.com/iot-lab/iot-lab.git'
    local cmd2='cd iot-lab; make setup-openlab'
    local cmd3='ln -nfsv iot-lab/parts/openlab/appli/education/distributed_algorithm/ .'

    run_ssh ${login} "${cmd1}"
    run_ssh ${login} "${cmd2}"
    run_ssh ${login} "${cmd3}"
}


setup_accounts()
{
    local baselogin=$1
    local num=$2

    for i in $(seq 1 ${num})
    do
        local login=${baselogin}${i}
        echo ${login}
        create_distributed_algorithm_link ${login}
    done
}


setup_accounts ${ACCOUNTS} ${NUM}
