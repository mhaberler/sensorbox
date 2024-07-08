<DashboardPage title="Hilmarogramm">

    <Card title="elevation/speed">

        <SvgChartUnit
            className="elevation-speed"
            subtopic="gpx/trackpoints"
            subconvert={JSONConvert((value, context) => {
                if (context)
                    console.log(context);
                return [value["speed"], value["ele"]];
            })}
        />
    </Card>
</DashboardPage >