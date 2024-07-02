<DashboardPage>

    <Card title="Hilmarogramm">

        <ScatterplotUnit
            subtopic="gpx/trackpoints"
            subconvert={JSONConvert(value => [value["speed"],value["ele"]])}
            
            width={700}
            height={400}
        />
    </Card>
</DashboardPage >