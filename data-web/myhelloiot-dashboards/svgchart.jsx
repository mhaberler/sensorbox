{  data = [
  {
    x: 2,
    y: 4,
  },
  {
    x: 3,
    y: 5,
  },
  {
    x: 1,
    y: 2,
  }
];
}

<DashboardPage>

    <Card title="speed/elevation">

        <SvgChartUnit
            data={data}
            // subtopic="gpx/trackpoints"
            // subconvert={JSONConvert(value => [value["speed"],value["ele"]])}

        />
    </Card>
</DashboardPage >