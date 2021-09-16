import paho.mqtt.client as mqtt
import datetime
import threading
import time

avg_latency = 0
total_msg_count = 0
test_topic = "/topic/test"

def on_message(client, userdata, message):
    # print("message topic=",message.topic)
    # print("message qos=",message.qos)
    # print("message retain flag=",message.retain)

    if message.topic == test_topic:
        global avg_latency
        global total_msg_count
        diff = int(round(time.time()*1000)) - int(message.payload)
        total_latency = avg_latency * total_msg_count + diff
        total_msg_count = total_msg_count + 1
        avg_latency = total_latency / total_msg_count     
        print("Total message count: ", str(total_msg_count), " | Average latency: ", str(avg_latency), "ms | Latest addition: ", str(diff), " ms")


def on_connect(client, userdata, flags, rc):
    print("connection return code: ", rc)

def send_ping_loop(client, topic):
    while (1): 
        send_time = round(time.time() * 1000)
        # print(int(send_time))
        client.publish(topic, send_time)
        time.sleep(0.5)

client = mqtt.Client("testing")
client.on_message = on_message
client.on_connect = on_connect
client.username_pw_set("BOxY3vXv9PTcxIWVGZlVso5HZNZaqAVRchD5AqoA48CrEnc8clTvyN0nqNnGb7ld", "")
client.connect("mqtt.flespi.io")
client.subscribe(test_topic)

# client.subscribe("/topic/red")
# client.subscribe("/topic/blue")
# client.subscribe("/topic/green")
# client.publish("/topic/green", "hi")

test_publish_thread = threading.Thread(target=send_ping_loop, args=(client, test_topic,))
test_publish_thread.start() 

client.loop_forever() 