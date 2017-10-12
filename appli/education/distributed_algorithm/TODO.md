Ideas of improvement
====================

1. Replace the 'auth-cli' in the tex file by a dedicated tutorial

2. Provide copy/paste-ready examples of commands used in the training

	ssh -X miscit10@grenoble.iot-lab.info

	experiment-cli reload -i 81521
	experiment-cli wait

	cd distributed_algorithm
	git diff

	vi clock_convergence.c
	git diff

	make
	node-cli --update distributed_algorithm.elf

	./algo_clock_convergence.py --duration 200 --lambda 0.2 -g results/communication_graph/neighbours.csv --outdir results/ex_3
	./plot_values.py results/ex_3/clock_all.csv 1,b 2,r

