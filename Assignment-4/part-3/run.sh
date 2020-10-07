# t1=$(date +%Y%m%d%H%M%S)
# echo $t1
for i in {1..50}
do  
    t1=$(date +%Y%m%d%H%M%S)
    # echo $i
    ./inputgen sample 1000000 800000
    ./serial_hash sample-800000.bin 800001 1 > true1.txt
    ./parallel_hash sample-800000.bin 800001 31 > output.txt
    awk '{print $1,$2,$3,$4,$6;}' true1.txt > true1.txt
    awk '{print $1,$2,$3,$4,$6;}' output.txt > output.txt
    diff true1.txt output.txt
    t2=$(date +%Y%m%d%H%M%S)
    echo "--------------"
    echo $(($t2 - $t1))
    echo "--------------"
done
# t2=$(date +%Y%m%d%H%M%S)
# echo $(($t2 - $t1))