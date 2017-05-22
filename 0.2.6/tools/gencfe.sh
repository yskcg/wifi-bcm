#!/bin/bash
if [ "$#" -eq "3" ];then
    	mkdir -p cfe
	dec2hex(){
	   printf "0x%03x" $1
	}
	for((i=0;i<$1;i++))
	do 
	hex=$(dec2hex $i)
	echo $hex
	./nvserial -i $2 -o cfe/cfe_$hex.bin -s $hex $3
	done
	echo "success"
else
    echo "Usage: gencfe.sh count cfez.bin bcm95357nr2_p163.txt "
    echo "for example: gencfe.sh 5 cfez-gmac_20.bin bcm95357nr2_p163_cm20_ok.txt"
fi


