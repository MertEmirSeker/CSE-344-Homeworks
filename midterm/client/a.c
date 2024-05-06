#include <stdio.h>

int main() {
    int number;
    printf("Bir sayı giriniz: ");
    scanf("%d", &number);

    int square = number * number;

    printf("Girilen sayının karesi: %d\n", square);
    
    return 0;
}

