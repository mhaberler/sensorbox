<DashboardPage title="Hilmarogramm">

    <Card title="elevation/speed">

        <SvgChartUnit
            className="elevation-speed"
            subtopic="gpx/trackpoints"
            subconvert={JSONConvert(value => [value["speed"], value["ele"]])}
        />
    </Card>

    <Card title="elevation/course">

        <SvgChartUnit
            className="elevation-course"
            subtopic="gpx/trackpoints"
            subconvert={JSONConvert(value => [value["course"], value["ele"]])}
            minX={0}
            maxX={360}
            yticklabels="false"
        />
    </Card>
</DashboardPage >