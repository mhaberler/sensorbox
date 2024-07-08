<DashboardPage>

    <Card title="speed/elevation">

        <ScatterplotUnit
            subtopic="gpx/trackpoints"
            subconvert={JSONConvert(value => [value["speed"],value["ele"]])}

        />
    </Card>
    <Card title="course/elevation">

       <ScatterplotUnit
            subtopic="gpx/trackpoints"
            subconvert={JSONConvert(value => [value["course"],value["ele"]])}

        />
    </Card>
</DashboardPage >

{


}