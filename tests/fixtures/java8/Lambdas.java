// Fixture (somente Java 8): invokedynamic, MethodHandle, BootstrapMethods e genericos.
interface Op {
    int apply(int x);
}

public class Lambdas {
    static java.util.List<String> names = new java.util.ArrayList<String>();

    public static void main(String[] args) {
        Op twice = x -> x * 2;
        Runnable r = () -> names.add("x");
        r.run();
        System.out.println(twice.apply(21));
    }
}
