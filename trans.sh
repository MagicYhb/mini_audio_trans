#!/bin/bash
 
#> File Name:        trans.sh
#> Author:           MagicYang
#> Version:          1.0.1
#> Mail:             476080754@qq.com
#> Created Time:     2022-03-11 15:08:35
 
SRC_FILE=$1
echo "src_file $SRC_FILE"
./audio_trans g711a ./qb_base_16K/xhr/$SRC_FILE opus
mv ./out.opus ./qb_base_opus_16K/xhr/$SRC_FILE
