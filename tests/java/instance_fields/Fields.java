// Atributos de instancia de todos os tipos, valores padrao e campos homonimos na hierarquia.
public class Fields {
    boolean z;
    byte b;
    char c;
    short s;
    int i;
    long l;
    float f;
    double d;
    Object o;
    int[] arr;

    public static void main(String[] args) {
        Fields x = new Fields();
        System.out.println(x.z);
        System.out.println(x.b);
        System.out.println((int) x.c);
        System.out.println(x.s);
        System.out.println(x.i);
        System.out.println(x.l);
        System.out.println(x.f);
        System.out.println(x.d);
        System.out.println(x.o);
        System.out.println(x.arr == null);
        x.z = true;
        x.b = -128;
        x.c = 'Z';
        x.s = -32768;
        x.i = 123456;
        x.l = -1234567890123L;
        x.f = 2.5f;
        x.d = -0.125;
        x.o = "texto";
        System.out.println(x.z);
        System.out.println(x.b);
        System.out.println(x.c);
        System.out.println(x.s);
        System.out.println(x.i);
        System.out.println(x.l);
        System.out.println(x.f);
        System.out.println(x.d);
        System.out.println(x.o);
        Sub sub = new Sub();
        Base base = sub;
        System.out.println(sub.value);
        System.out.println(base.value);
        System.out.println(sub.baseValue());
        System.out.println(sub.both());
    }
}

class Base {
    int value = 1;

    int baseValue() {
        return value;
    }
}

class Sub extends Base {
    int value = 2;

    int both() {
        return value * 10 + super.value;
    }
}
