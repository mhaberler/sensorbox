import gpxpy
import paho.mqtt.client as mqtt
import time
import math
import sys

# Configuration
gpx_file = 'track.gpx' #  GPX file
MQTT_BROKER = 'localhost'  # MQTT broker address
MQTT_PORT = 1883  # MQTT broker port
MQTT_TOPIC = 'gpx/trackpoints'  # MQTT topic
delay = 0.1  # between publishing track points

# Haversine formula to calculate distance between two points on the Earth
def haversine(lat1, lon1, lat2, lon2):
    R = 6371e3  # Earth radius in meters
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    delta_phi = math.radians(lat2 - lat1)
    delta_lambda = math.radians(lon2 - lon1)
    a = math.sin(delta_phi / 2) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(delta_lambda / 2) ** 2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    distance = R * c
    return distance

# Function to calculate course (bearing) between two points
def calculate_course(lat1, lon1, lat2, lon2):
    delta_lon = math.radians(lon2 - lon1)
    lat1 = math.radians(lat1)
    lat2 = math.radians(lat2)
    x = math.sin(delta_lon) * math.cos(lat2)
    y = math.cos(lat1) * math.sin(lat2) - (math.sin(lat1) * math.cos(lat2) * math.cos(delta_lon))
    initial_bearing = math.atan2(x, y)
    initial_bearing = math.degrees(initial_bearing)
    compass_bearing = (initial_bearing + 360) % 360
    return compass_bearing


# Read GPX File
def read_gpx_file(file_path):
    with open(file_path, 'r') as gpx_file:
        gpx = gpxpy.parse(gpx_file)
    return gpx

# Publish track points to MQTT
def publish_track_points(gpx, client):
    for track in gpx.tracks:
        for segment in track.segments:
            previous_point = None
            for point in segment.points:
                if previous_point is not None:
                    time_diff = (point.time - previous_point.time).total_seconds()
                    if time_diff > 0:
                        distance = haversine(previous_point.latitude, previous_point.longitude, point.latitude, point.longitude)
                        speed = distance / time_diff  # speed in meters/second
                        course = calculate_course(previous_point.latitude, previous_point.longitude, point.latitude, point.longitude)
                    else:
                        speed = 0
                        course = None
                else:
                    speed = 0
                    course = None

                payload = {
                    'lat': point.latitude,
                    'lon': point.longitude,
                    'ele': point.elevation,
                    'time': point.time.timestamp() if point.time else None,
                }
                if speed:
                    payload['speed'] = round(speed,2)
                if course:
                    payload['course'] = round(course,2)

                client.publish(MQTT_TOPIC, str(payload))
                # print(f"Published: {payload}")
                time.sleep(delay)  # Delay to simulate real-time publishing
                previous_point = point


def main():
    # Initialize MQTT Client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()

    # Read GPX data
    gpx = read_gpx_file(gpx_file)

    # Publish track points
    publish_track_points(gpx, client)

    # Disconnect MQTT Client
    client.loop_stop()
    client.disconnect()

if __name__ == '__main__':
    main()
