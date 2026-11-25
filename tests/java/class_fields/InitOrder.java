// <clinit> executa uma unica vez, na primeira utilizacao ativa, superclasse antes da subclasse.
public class InitOrder {
    public static void main(String[] args) {
        System.out.println("main comecou");
        System.out.println(C.c);
        System.out.println(B.b);
        new C();
        System.out.println(A.a);
    }
}

class A {
    static int a = InitOrder2.log("A", 1);
}

class B extends A {
    static int b = InitOrder2.log("B", 2);
}

class C extends B {
    static int c = InitOrder2.log("C", 3);
}

class InitOrder2 {
    static int log(String s, int v) {
        System.out.println("clinit " + s);
        return v;
    }
}
