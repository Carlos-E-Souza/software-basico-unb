// Excecoes: try/catch/finally aninhados, propagacao entre metodos, excecoes da JVM e do usuario.
public class TryCatch {
    static int depth = 0;

    static int divide(int a, int b) {
        return a / b;
    }

    static void level3(int v) throws MyException {
        if (v > 2) {
            throw new MyException("valor alto: " + v, v);
        }
        System.out.println("level3 ok " + v);
    }

    static void level2(int v) throws MyException {
        try {
            level3(v);
        } finally {
            System.out.println("finally level2 " + v);
        }
    }

    static int withFinally() {
        try {
            return 1;
        } finally {
            System.out.println("finally antes do return");
        }
    }

    static int recurse(int n) {
        depth++;
        return recurse(n + 1) + 1;
    }

    public static void main(String[] args) {
        try {
            System.out.println(divide(10, 0));
        } catch (ArithmeticException e) {
            System.out.println("capturou: " + e.getMessage());
        }
        try {
            long z = 0;
            System.out.println(5L % z);
        } catch (ArithmeticException e) {
            System.out.println("long: " + e);
        }
        try {
            int[] arr = new int[2];
            arr[5] = 1;
        } catch (ArrayIndexOutOfBoundsException e) {
            System.out.println("indice: " + e.getMessage());
        }
        try {
            String s = null;
            s.length();
        } catch (NullPointerException e) {
            System.out.println("NPE capturada " + (e.getMessage() == null));
        }
        try {
            Object o = "texto";
            Integer_like x = (Integer_like) o;
            System.out.println(x);
        } catch (ClassCastException e) {
            System.out.println("cast: " + e.getMessage());
        }
        try {
            int[] neg = new int[-1];
            System.out.println(neg.length);
        } catch (NegativeArraySizeException e) {
            System.out.println("negativo: " + e.getMessage());
        }
        for (int i = 1; i <= 3; i++) {
            try {
                level2(i);
            } catch (MyException e) {
                System.out.println("MyException " + e.getMessage() + " codigo " + e.code);
            }
        }
        System.out.println(withFinally());
        try {
            try {
                throw new IllegalStateException("interna");
            } catch (IllegalArgumentException e) {
                System.out.println("nao deveria");
            } finally {
                System.out.println("finally interno");
            }
        } catch (RuntimeException e) {
            System.out.println("externo pegou " + e);
        }
        try {
            throw new Error("um erro");
        } catch (Throwable t) {
            System.out.println("Throwable: " + t.getMessage());
        }
        try {
            recurse(0);
        } catch (StackOverflowError e) {
            System.out.println("stack overflow capturado " + (depth > 100));
        }
        try {
            "abc".charAt(10);
        } catch (StringIndexOutOfBoundsException e) {
            System.out.println(e.getMessage());
        }
        try {
            throw new MyException("rethrow", 9);
        } catch (Exception e) {
            if (e instanceof MyException) {
                System.out.println("instanceof MyException: " + ((MyException) e).code);
            }
        }
        System.out.println("fim");
    }
}

class MyException extends Exception {
    int code;

    MyException(String message, int code) {
        super(message);
        this.code = code;
    }
}

class Integer_like {
}
