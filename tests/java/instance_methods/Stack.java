// Metodos de instancia: pilha encadeada e lista dinamica com redimensionamento de array.
public class Stack {
    private Node top;
    private int size;

    void push(int v) {
        top = new Node(v, top);
        size++;
    }

    int pop() {
        int v = top.value;
        top = top.next;
        size--;
        return v;
    }

    boolean isEmpty() {
        return top == null;
    }

    int size() {
        return size;
    }

    public static void main(String[] args) {
        Stack s = new Stack();
        for (int i = 1; i <= 5; i++) {
            s.push(i * i);
        }
        System.out.println(s.size());
        while (!s.isEmpty()) {
            System.out.print(s.pop() + " ");
        }
        System.out.println();
        IntList list = new IntList();
        for (int i = 0; i < 20; i++) {
            list.add(i * 3);
        }
        System.out.println(list.size() + " " + list.get(19) + " " + list.capacity());
        System.out.println(list.contains(27) + " " + list.contains(28));
        System.out.println(list);
    }
}

class Node {
    int value;
    Node next;

    Node(int value, Node next) {
        this.value = value;
        this.next = next;
    }
}

class IntList {
    private int[] data = new int[2];
    private int count;

    void add(int v) {
        if (count == data.length) {
            int[] bigger = new int[data.length * 2];
            for (int i = 0; i < count; i++) {
                bigger[i] = data[i];
            }
            data = bigger;
        }
        data[count++] = v;
    }

    int get(int i) {
        return data[i];
    }

    int size() {
        return count;
    }

    int capacity() {
        return data.length;
    }

    boolean contains(int v) {
        for (int i = 0; i < count; i++) {
            if (data[i] == v) {
                return true;
            }
        }
        return false;
    }

    public String toString() {
        StringBuffer sb = new StringBuffer("[");
        for (int i = 0; i < count; i++) {
            if (i > 0) {
                sb.append(", ");
            }
            sb.append(data[i]);
        }
        return sb.append("]").toString();
    }
}
