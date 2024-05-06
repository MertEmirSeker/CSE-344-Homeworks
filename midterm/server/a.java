import java.util.Scanner;
ERKAN ZERGEROGLU
public class SquareCalculator {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);

        System.out.println("Bir sayı giriniz: ");
        int number = scanner.nextInt();
        
        int square = number * number;
        
        System.out.println("Girilen sayının karesi: " + square);
        
        scanner.close();
    }
}

YENİ
