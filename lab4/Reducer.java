package sjtu.sdic.mapreduce.core;

import com.alibaba.fastjson.JSON;
import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import org.codehaus.jackson.JsonParser;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.common.Utils;

import javax.rmi.CORBA.Util;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

/**
 * Created by Cachhe on 2019/4/19.
 */
public class Reducer {

    /**
     *
     * 	doReduce manages one reduce task: it should read the intermediate
     * 	files for the task, sort the intermediate key/value pairs by key,
     * 	call the user-defined reduce function {@code reduceFunc} for each key,
     * 	and write reduceFunc's output to disk.
     *
     * 	You'll need to read one intermediate file from each map task;
     * 	{@code reduceName(jobName, m, reduceTask)} yields the file
     * 	name from map task m.
     *
     * 	Your {@code doMap()} encoded the key/value pairs in the intermediate
     * 	files, so you will need to decode them. If you used JSON, you can refer
     * 	to related docs to know how to decode.
     *
     *  In the original paper, sorting is optional but helpful. Here you are
     *  also required to do sorting. Lib is allowed.
     *
     * 	{@code reduceFunc()} is the application's reduce function. You should
     * 	call it once per distinct key, with a slice of all the values
     * 	for that key. {@code reduceFunc()} returns the reduced value for that
     * 	key.
     *
     * 	You should write the reduce output as JSON encoded KeyValue
     * 	objects to the file named outFile. We require you to use JSON
     * 	because that is what the merger than combines the output
     * 	from all the reduce tasks expects. There is nothing special about
     * 	JSON -- it is just the marshalling format we chose to use.
     *
     * 	Your code here (Part I).
     *
     *
     * @param jobName the name of the whole MapReduce job
     * @param reduceTask which reduce task this is
     * @param outFile write the output here
     * @param nMap the number of map tasks that were run ("M" in the paper)
     * @param reduceFunc user-defined reduce function
     */
    public static void doReduce(String jobName, int reduceTask, String outFile, int nMap, ReduceFunc reduceFunc) {
        Map<String,String[]> keyvalues_map = new HashMap<>();
        for (int i = 0; i < nMap; i++) {
            List<KeyValue> keyvalue_list;
            String inter_file_name = Utils.reduceName(jobName,i,reduceTask);
            String json_string = "";

            try{
                FileInputStream fi = new FileInputStream(inter_file_name);
                InputStreamReader ir = new InputStreamReader(fi);
                BufferedReader br = new BufferedReader(ir);

                String tempString = null;
                while((tempString = br.readLine()) != null)
                {
                    json_string += tempString;
                }

                br.close();
                ir.close();
                fi.close();
            }
            catch (IOException e){
                e.printStackTrace();
            }

            keyvalue_list = JSON.parseArray(json_string,KeyValue.class);

            for (int j = 0; j < keyvalue_list.size(); j++) {
                KeyValue kv_pair = keyvalue_list.get(j);
                if(keyvalues_map.containsKey(kv_pair.key)){
                    String[] values = keyvalues_map.get(kv_pair.key);
                    int length = values.length;
                    String[] new_values = new String[length + 1];

                    System.arraycopy(values,0,new_values,0,length);
                    new_values[length] = kv_pair.value;
                    keyvalues_map.put(kv_pair.key,new_values);
                }
                else
                {
                    String[] value = new String[1];
                    value[0] = kv_pair.value;
                    keyvalues_map.put(kv_pair.key,value);
                }
            }
        }

        File merge_file = new File(outFile);
        if(!merge_file.exists()){
            try {
                merge_file.createNewFile();
            }
            catch (IOException e){
                e.printStackTrace();
            }

        }

        Map<String,String> reduced_map = new TreeMap<>();
        for(Map.Entry<String,String[]> entry:keyvalues_map.entrySet()){
            String reduced_value = reduceFunc.reduce(entry.getKey(),entry.getValue());
            KeyValue temp_kv  = new KeyValue(entry.getKey(),reduced_value);
            reduced_map.put(temp_kv.key,temp_kv.value);
        }

        String json_string = JSON.toJSONString(reduced_map);
        try {
            FileWriter fw = new FileWriter(outFile);
            BufferedWriter bw = new BufferedWriter(fw);
            PrintWriter pw = new PrintWriter(bw);
            pw.write(json_string);

            pw.close();
            bw.close();
            fw.close();
        }
        catch (IOException e){
            e.printStackTrace();
        }

    }
}
