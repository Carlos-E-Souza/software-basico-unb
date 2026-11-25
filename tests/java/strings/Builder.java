// Strings: StringBuilder/StringBuffer explicitos, comparacao, busca e montagem de texto em lacos.
public class Builder {
    static String repeat(String s, int n) {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < n; i++) {
            sb.append(s);
        }
        return sb.toString();
    }

    static String reverse(String s) {
        String r = "";
        for (int i = s.length() - 1; i >= 0; i--) {
            r = r + s.charAt(i);
        }
        return r;
    }

    static int count(String s, char c) {
        int n = 0;
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) == c) {
                n++;
            }
        }
        return n;
    }

    static boolean isPalindrome(String s) {
        return s.equals(reverse(s));
    }

    public static void main(String[] args) {
        System.out.println(repeat("ab", 3));
        System.out.println(reverse("maquina virtual"));
        System.out.println(count("banana", 'a'));
        System.out.println(isPalindrome("arara"));
        System.out.println(isPalindrome("java"));
        String csv = "um,dois,tres";
        int start = 0;
        int comma = csv.indexOf(",");
        while (comma >= 0) {
            System.out.println(csv.substring(start, comma));
            csv = csv.substring(comma + 1);
            comma = csv.indexOf(",");
        }
        System.out.println(csv);
        StringBuffer table = new StringBuffer("tabela:");
        for (int i = 1; i <= 3; i++) {
            table.append(' ').append(i).append('x').append(i * 1.5).append('=').append(i * i * 1.5f);
        }
        System.out.println(table);
        String a = "jvm";
        String b = "j" + "vm";
        String c = new String("jvm");
        System.out.println(a == b);
        System.out.println(a == c);
        System.out.println(a.equals(c));
        System.out.println(a.hashCode() == c.hashCode());
        char[] chars = {'x', 'y'};
        System.out.println(String.valueOf(chars) + String.valueOf(10L) + String.valueOf(2.5f));
        Object o = new StringBuffer("sb");
        System.out.println("objeto: " + o);
    }
}
