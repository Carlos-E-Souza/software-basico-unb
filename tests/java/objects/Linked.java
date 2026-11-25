// Objetos: lista ligada, identidade x igualdade, objetos como parametros e retornos, null e toString padrao omitido.
public class Linked {
    static Person oldest(Person[] ps) {
        Person best = null;
        for (int i = 0; i < ps.length; i++) {
            if (best == null || ps[i].age > best.age) {
                best = ps[i];
            }
        }
        return best;
    }

    static void birthday(Person p) {
        p.age++;
    }

    static void reassign(Person p) {
        p = new Person("Outro", 0);
        p.age = 99;
    }

    public static void main(String[] args) {
        Person[] ps = new Person[3];
        ps[0] = new Person("Ana", 30);
        ps[1] = new Person("Bruno", 45);
        ps[2] = new Person("Carla", 27);
        Person o = oldest(ps);
        System.out.println(o.name + " " + o.age);
        birthday(o);
        System.out.println(ps[1].age);
        reassign(o);
        System.out.println(o.name + " " + o.age);
        Person copy = new Person("Ana", 30);
        System.out.println(copy == ps[0]);
        System.out.println(copy.sameAs(ps[0]));
        Person chain = null;
        for (int i = 0; i < ps.length; i++) {
            ps[i].friend = chain;
            chain = ps[i];
        }
        int n = 0;
        for (Person p = chain; p != null; p = p.friend) {
            System.out.print(p.name + " ");
            n++;
        }
        System.out.println(n);
        Object obj = new Object();
        System.out.println(obj.equals(obj) + " " + obj.equals(new Object()));
        System.out.println(obj.hashCode() == obj.hashCode());
    }
}

class Person {
    String name;
    int age;
    Person friend;

    Person(String name, int age) {
        this.name = name;
        this.age = age;
    }

    boolean sameAs(Person other) {
        return other != null && name.equals(other.name) && age == other.age;
    }
}
