//
//  Input Test.c
//  Shell
//
//  Created by Duke Kim on 10/5/13.
//  Copyright (c) 2013 Duke Kim. All rights reserved.
//

#include <stdio.h>
int main(){
    int num = 0;
    int x = 0;
    while(scanf("%d", &num) != EOF){
        printf("number is %d\n",num);
        x+=num;
    }
    printf("%d",145);
}