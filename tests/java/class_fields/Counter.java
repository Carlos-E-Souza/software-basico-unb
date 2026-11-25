// Atributos de classe: contador estatico compartilhado, inicializacao estatica e constantes.
public class Counter {
    static int count;
    static long total = 5000000000L;
    static double ratio = 1.5;
    static final int LIMIT = 3;
    static String name = "contador";
    static int[] table;
    static int initOrder;

    static {
        table = new int[LIMIT];
        for (int i = 0; i < LIMIT; i++) {
            table[i] = i * i;
        }
        initOrder = 42;
    }

    int id;

    Counter() {
        count++;
        id = count;
        total += 10;
    }

    public static void main(String[] args) {
        System.out.println(count);
        new Counter();
        new Counter();
        Counter c = new Counter();
        System.out.println(count);
        System.out.println(c.id);
        System.out.println(total);
        System.out.println(ratio * LIMIT);
        System.out.println(name);
        System.out.println(table[2]);
        System.out.println(initOrder);
        System.out.println(Other.value);
        System.out.println(Other.value);
        Other.value = 7;
        System.out.println(Other.value);
    }
}

class Other {
    static int value;

    static {
        System.out.println("inicializando Other");
        value = 3;
    }
}
