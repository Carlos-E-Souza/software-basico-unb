// Heranca, polimorfismo e reescrita: Animal -> Cachorro/Gato, super.metodo(), construtores encadeados.
public class Animals {
    public static void main(String[] args) {
        Animal[] animals = new Animal[3];
        animals[0] = new Cachorro("Rex");
        animals[1] = new Gato("Mimi");
        animals[2] = new Animal("Generico");
        for (int i = 0; i < animals.length; i++) {
            System.out.println(animals[i].describe());
        }
        Cachorro c = (Cachorro) animals[0];
        c.fetch();
        System.out.println(animals[1] instanceof Gato);
        System.out.println(animals[1] instanceof Cachorro);
        System.out.println(animals[1] instanceof Animal);
        System.out.println(Animal.created);
    }
}

class Animal {
    static int created;
    protected String name;

    Animal(String name) {
        this.name = name;
        created++;
    }

    String sound() {
        return "...";
    }

    String describe() {
        return name + " faz " + sound();
    }
}

class Cachorro extends Animal {
    Cachorro(String name) {
        super(name);
    }

    String sound() {
        return "au au";
    }

    void fetch() {
        System.out.println(name + " buscou a bola");
    }
}

class Gato extends Animal {
    Gato(String name) {
        super(name);
    }

    String sound() {
        return "miau";
    }

    String describe() {
        return "Gato " + super.describe();
    }
}
