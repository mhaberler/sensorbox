<DashboardPage title="Uplot & Leaflet demo">
    
    <Card title="Map">

        <LeafletUnit
            subtopic="gpx/trackpoints"
            subconvert={
                ((b) => {
                    // return [47,15];
                    return L.latLng(47,15)

                })
            }
        />
    </Card>


</DashboardPage >
