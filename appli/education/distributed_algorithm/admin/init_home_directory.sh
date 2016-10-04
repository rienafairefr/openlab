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
    local cmd='ln -nfsv iot-lab/parts/openlab/appli/education/distributed_algorithm/ .'

    run_ssh ${login} "${cmd}"
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
