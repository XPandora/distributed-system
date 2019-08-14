package sjtu.sdic.mapreduce;

import org.apache.commons.io.filefilter.WildcardFileFilter;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.core.Master;
import sjtu.sdic.mapreduce.core.Worker;

import java.io.File;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Created by Cachhe on 2019/4/24.
 */
public class InvertedIndex {

    public static List<KeyValue> mapFunc(String file, String value) {

        List<KeyValue> kv_list = new ArrayList<>();
        Pattern p  = Pattern.compile("[a-zA-Z0-9]+");
        Matcher m = p.matcher(value);
        while(m.find())
        {
            KeyValue kv_pair = new KeyValue(m.group(),file);
            System.out.println(kv_pair.key);
            kv_list.add(kv_pair);
        }

        return  kv_list;
    }

    public static String reduceFunc(String key, String[] values) {
        List<String> value_list = new ArrayList<>();

        for (int i = 0; i <values.length ; i++) {
            if(!value_list.contains(values[i])) {
                value_list.add(values[i]);
            }
        }
        String output = Integer.toString(value_list.size());
        output+= " ";
        for (int i = 0; i < value_list.size(); i++) {
            output += value_list.get(i);
            if(i != value_list.size() - 1){
                output += ",";
            }
        }

        return output;
    }

    public static void main(String[] args) {
        if (args.length < 3) {
            System.out.println("error: see usage comments in file");
        } else if (args[0].equals("master")) {
            Master mr;

            String src = args[2];
            File file = new File(".");
            String[] files = file.list(new WildcardFileFilter(src));
            if (args[1].equals("sequential")) {
                mr = Master.sequential("iiseq", files, 3, InvertedIndex::mapFunc, InvertedIndex::reduceFunc);
            } else {
                mr = Master.distributed("iiseq", files, 3, args[1]);
            }
            mr.mWait();
        } else {
            Worker.runWorker(args[1], args[2], InvertedIndex::mapFunc, InvertedIndex::reduceFunc, 100, null);
        }
    }
}
