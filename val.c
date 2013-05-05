#include <stdio.h>

typedef enum {false, true};

int validate(const char *str) {
  //assert first char and last char are "
  char *s ;
  s =  str;
  int preceed = false; //true if we have just encountered a separator
  int ignore  = true;  //true if we're outside a list entry
  while(*s) {
    if(*s!='\\') {
      if(ignore) {
        //Should be strictly in [ ,"{}]
        //INSIDE IGNORE ZONE
          //three cases:
          //  1. first zone  [ {]  (NO MORE POSSIBLE)
          //  2. middle zone [", ]
          //  3. end zone [} ]     (NO MORE POSSIBLE)
          1;
       } else {
        //OUTSIDE IGNORE ZONE (aka INSIDE LIST ELEMENT)
         if(preceed == true) {
           //SHOULD BE [",]
           //, should be crossed once
         } else {
           if(*s=="\"") {
             if(ignore) {
                ignore=false;
                //leaving zone 1 or 2
             } else {
                ignore=true;
                //entering zone 2 or 3
             }
           } else {
           }
         }
      }
    } else {
      if(preceed) {
        //SHOULD BE [\\]
         preceed = false;
      } else {
         preceed = true;
      }
    }
    s++;
  }
  return 0;
}

int main() {
  char str[] = "  { \"  XX\\\"XX  \"  ,  \"   YY\\,Y  \"     ,     \"  ZZZ \"        }     ";
  int ret;
  ret = validate(str);
  printf("ret=%d\n", ret);
  return 0;
}

