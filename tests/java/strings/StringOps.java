// Strings: concatenacao com todos os tipos, metodos de String, StringBuilder e literais internados.
public class StringOps {
    public static void main(String[] args) {
        String s = "Software";
        String t = "Basico";
        System.out.println(s + " " + t);
        System.out.println(s.length());
        System.out.println(s.charAt(4));
        System.out.println(s.substring(4));
        System.out.println(s.substring(0, 4));
        System.out.println(s.indexOf('w'));
        System.out.println(s.indexOf("ware"));
        System.out.println(s.indexOf("xyz"));
        System.out.println(s.concat(t));
        System.out.println(s.equals("Software"));
        System.out.println(s.equals(t));
        System.out.println(s == "Software");
        System.out.println("abc".hashCode());
        System.out.println(String.valueOf(42));
        System.out.println(String.valueOf(3.5));
        System.out.println(String.valueOf(true));
        System.out.println(String.valueOf('c'));
        int i = 7;
        long l = 8000000000L;
        float f = 1.5f;
        double d = 2.75;
        char c = 'x';
        boolean b = false;
        Object nothing = null;
        String all = "i=" + i + " l=" + l + " f=" + f + " d=" + d + " c=" + c + " b=" + b + " n=" + nothing;
        System.out.println(all);
        System.out.println(1 + 2 + "3" + 4 + 5);
        String acc = "";
        for (int k = 0; k < 5; k++) {
            acc += k;
        }
        System.out.println(acc);
        StringBuffer sb = new StringBuffer();
        sb.append("x").append(1).append('y').append(2.0).append(true);
        System.out.println(sb.toString());
        System.out.println(sb.length());
        char[] letters = {'o', 'l', 'a'};
        String fromChars = new String(letters);
        System.out.println(fromChars);
        String copy = new String("copia");
        System.out.println(copy.equals("copia"));
        System.out.println("acentuação: é ç ã");
        System.out.println("".length());
        String n = null;
        System.out.println("null? " + n);
    }
}
