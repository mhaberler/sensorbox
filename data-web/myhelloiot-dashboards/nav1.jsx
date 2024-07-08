<DashboardPage title="NavUnit">

    <Card>
        <NavUnit
            subtopic="gpx/trackpoints"
            select={((m, h) => {
                try {
                    const j = JSON.parse(m.message?.toString("utf8"));
                    return [m.time, { lat: j.lat ?, lng: j.lon ?, alt: j.hAE ?, speed: j.gSpeed ?, course: j.headMot ?}];
                } catch (exception) {
                    return null;
                }
            });
            }
        />
    </Card>
</DashboardPage >


{
    // select={JSONConvert((value, context) => {
    //     if (context)
    //         console.log(context);
    //     return [value["speed"], value["ele"]];
    // })}
}