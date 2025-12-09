#include "..\vendor\fluxdb\fluxdb_client.hpp"

int main() {
    fluxdb::FluxDBClient client("127.0.0.1", 8080);
    
    if (!client.auth("flux_admin")) return 1;
    client.use("crm_db");

    fluxdb::Document lead;
    lead["name"] = std::make_shared<fluxdb::Value>("Alice Corp");
    lead["status"] = std::make_shared<fluxdb::Value>("Lead");
    lead["value"] = std::make_shared<fluxdb::Value>(5000);
    
    client.insert(lead);

    fluxdb::Document query;
    
    fluxdb::Document range;
    range["$gt"] = std::make_shared<fluxdb::Value>(1000);
    query["value"] = std::make_shared<fluxdb::Value>(range);

    auto results = client.find(query);
    
    std::cout << "Found " << results.size() << " high value leads.\n";
    
    return 0;
}