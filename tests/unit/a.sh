#!/bin/bash
#
# Expecting the following values in the list:
#
# {{abc}}
#  ' def}
# ghi'''
#
echo "@myliste={\" \\\" \"}" 

echo "@mylista={\"\"}" 
echo "@mylistb={\"\",\"\"}" 
echo "@mylistc={\"a\"}" 
echo "@mylistd={\"a\",\"b\"}" 

echo "@myliste={\"\\\\,\"}" 
echo "@mylistf={\"{}\"}" 
echo "@mylistg={\"\\\\\"}" 
echo "@mylisth={\" \\\\ \"}" 
echo "@mylist=   {\"{{ab\\,c}}\", \"  ' d\\\"ef}\", \"ghi'''\"}   " 
echo "@mybist= {\"{{abc}}\", \"  ' def}\", \"ghi'''\"}" 
echo "@mykist={\"{{abc}}\", \"  ' def}\", \"ghi'''\"}   " 
