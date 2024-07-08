<DashboardPage title="Chart example">
  <Card title="Sets chart data">
    <InputUnit
      topic="myhelloiot/chartexample"
    />   
  </Card>
  <Card title="Chart">
    <ViewUnit
      topic="myhelloiot/chartexample"
      format={ChartIconFormat({
        style: {
          data: {
            fill: "#0019ac66",
            stroke: "#0019ac",
            strokeWidth: 2,
            strokeLinecap: "round"
          },
        },
        domain: { y: [0, 100] }
      })}
    />     
  </Card>
</DashboardPage>