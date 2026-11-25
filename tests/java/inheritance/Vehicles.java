// Heranca em 3 niveis: campos e metodos herdados, protected, construtores encadeados e estaticos herdados.
public class Vehicles {
    public static void main(String[] args) {
        Car car = new Car("Fusca", 1970, 4);
        SportsCar sport = new SportsCar("Ferrari", 2020, 2, 340);
        System.out.println(car.info());
        System.out.println(sport.info());
        System.out.println(sport.wheels());
        System.out.println(sport.age(2026));
        Vehicle v = sport;
        System.out.println(v.info());
        System.out.println(Vehicle.count + " " + Car.count + " " + SportsCar.count);
        System.out.println(sport instanceof Car);
        System.out.println(car instanceof SportsCar);
    }
}

class Vehicle {
    static int count;
    protected String model;
    protected int year;

    Vehicle(String model, int year) {
        this.model = model;
        this.year = year;
        count++;
        System.out.println("Vehicle(" + model + ")");
    }

    int age(int now) {
        return now - year;
    }

    int wheels() {
        return 0;
    }

    String info() {
        return model + " " + year + " rodas=" + wheels();
    }
}

class Car extends Vehicle {
    protected int doors;

    Car(String model, int year, int doors) {
        super(model, year);
        this.doors = doors;
        System.out.println("Car(" + doors + ")");
    }

    int wheels() {
        return 4;
    }

    String info() {
        return super.info() + " portas=" + doors;
    }
}

class SportsCar extends Car {
    private int speed;

    SportsCar(String model, int year, int doors, int speed) {
        super(model, year, doors);
        this.speed = speed;
        System.out.println("SportsCar(" + speed + ")");
    }

    String info() {
        return super.info() + " km/h=" + speed;
    }
}
