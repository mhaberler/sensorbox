<DashboardPage title="Hilmarogramm">

    <Card title="elevation history">

        <ViewHistoryUnit
            className="elevation-history"
            subtopic="gpx/trackpoints"
            windowsize={5}
            minDelay={3000}
            children={<>
                <SvgChartUnit
                    className="elevation-speed"
                    subtopic="gpx/trackpoints"
                    subconvert={JSONConvert(value => [value["speed"], value["ele"]])}
                />
                <SvgChartUnit
                    className="elevation-course"
                    subtopic="gpx/trackpoints"
                    subconvert={JSONConvert(value => [value["course"], value["ele"]])}
                    minX={0}
                    maxX={360}
                    yticklabels="false"
                />
            </>
            }
        >
        </ViewHistoryUnit>

    </Card>
</DashboardPage >

{
    //     <MapContainer
    //     center={{ lat: 47, lon: 15 }}
    //     zoom={9}
    //     scrollWheelZoom={false}
    //     style={{ minHeight: "50vh", minWidth: "30vw" }}
    // >
    //     <TileLayer url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
    //     />
    // </MapContainer>



    // select={JSONConvert((value, context) => {
    //     if (context)
    //         console.log(context);
    //     return [value["speed"], value["ele"]];
    // })}

    // select={((m, h) => {
    //     const json = JSON.parse(m.message?.toString("utf8"));
    //     return [m.time, json];
    //     // return { time: 0, value: 4};
    // })}
}