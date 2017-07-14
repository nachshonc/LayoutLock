echo "Thanks for running DLTree measurements. "
echo "To varies the parameters of the execution, set the parameters inside this script."
echo "Explanations appears inside the script"
echo -e "\n\n\n"
#Parameter 1: PGM. Put the set of programs (algorithms) to compare, space separated.  
#lb-bst_bronson, lb-bst-drachsler, lf-bst-aravind, lf-bst_ellen: previous work
#lf-bst-cohen: dltree (with layout lock). lf-bst-rwcohen: dltree with read-write lock. lf-bst-seqlock-cohen: dltree with seqlock
PGM="lb-bst_bronson lb-bst-drachsler lf-bst-aravind lf-bst_ellen lf-bst-cohen lf-bst-seqlock-cohen lf-bst-rwcohen"
#Parameter 2: TREE_SIZES. Put the expected size of the tree, comma separated. Every unit is 1000 items. 
#This parameter is used to compute the range. E.g., for 64, the range will be 128K, so the expected size is 64K.
TREE_SIZES="4 16 64 1024"   #To reproduce Figure 4 in the paper
#Parameter 3: Number of threads. Put the number of threads you are interested in. 
THREADS="1 2 4 8 16 32 64"
#Parameter 4: Time for each experiment in milli seconds. The paper used 5000 (5 seconds)
TIME=5000 #set to 1000 for faster testing.
#Parameter 5: Number of times each experiment is run. The final report is the average. 
ITERS=2 #The paper used 10. Set to a lower number to reduce running time (at the expense of somewhat higher variability)

echo "Running experiments. Showing progress as we go."
for RR in $TREE_SIZES; do 
	echo "RR=${RR}"; 
	for it in `seq 1 $ITERS` ; do 
		echo "RR=${RR}. Iteration=${it}"; 
		for t in $THREADS; do 
			echo "RR=${RR}, it=${it}, t=${t}"; 
			for pgm in $PGM; do 
				numactl --interleave=all ./${pgm} -n ${t} -i $((RR*1000)) -r $((RR*2000)) -d ${TIME} >> res_${pgm}_${t}_${RR}K.txt; 
			done; 
		done; 
	done; 
done

#parse
echo -e "\n\n"
echo "Finished to run experiments, now parsing"
PGM_RES=${PGM// /_RES_.txt }_RES_.txt
PGM_TABS="${PGM// /\t}"
PGM_COM="${PGM// /,}"

for t in $THREADS; do 
	echo $t >> THREADS.txt
done

cp plot.plt curplot.plt
for pgm in $PGM; do 
	echo -e "'results.dat' using (column(\"THREADS\")):(column(\"${pgm}\")) title \"${pgm}\" , \\"
done >> curplot.plt

#echo $PGM_RES
#SZ=64K; #Or put another size for the tree
for RR in $TREE_SIZES; do
	SZ=${RR}K 
	for pgm in $PGM; do 
		for t in $THREADS; do 
			cat res_${pgm}_${t}_${SZ}.txt | grep Mops | cut -d' ' -f2 | awk '{n++}{sum+=$1}END{printf ("%.3f\n", sum/n)}'; 
		done > ${pgm}_RES_.txt; 
	done; 
	printf "Showing raw data for tree size %dK\n", $RR
	paste $PGM_RES
	echo THREADS,"$PGM_COM" > results.dat
	paste -d, THREADS.txt $PGM_RES >> results.dat
	gnuplot curplot.plt
	mv results.eps GraphTreeSize${RR}K.eps
	mv results.dat RawDataTreeSize${RR}K.dat
done
echo "Finished to parse."
echo "Please look at GraphTreeSize*.eps for graphs and RawDataTreeSize*.dat for comma separated raw data"
rm curplot.plt 
rm THREADS.txt
rm res_*.txt
rm *_RES_.txt
