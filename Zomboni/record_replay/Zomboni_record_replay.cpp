#include "../../inc/tools/Zomboni.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;

    std::string out = "\"";
    for (char ch : s) {
        if (ch == '"') out += "\"\"";
        else out += ch;
    }
    out += "\"";
    return out;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: Zomboni_record_replay <batch.json> <results.csv>" << std::endl;
        return 1;
    }
    std::string batch_path = argv[1];
    std::string output_path = argv[2];

    try {
        std::ifstream fin(batch_path);
        if (!fin) throw std::runtime_error("cannot open batch json: " + batch_path);
        json batch;
        fin >> batch;
        const json& cases = batch.at("cases");

        std::ofstream fout(output_path);
        if (!fout) throw std::runtime_error("cannot open result csv: " + output_path);

        fout << "\xEF\xBB\xBF";
        fout << "case_id,僵尸波次,僵尸路,碾率,error\n";
        fout << std::fixed << std::setprecision(10);

        for (const auto& item : cases) {
            std::string case_id = item.at("case_id").get<std::string>();
            const json& meta = item.at("meta");
            long long zombie_wave = meta.at("zombie_wave").get<long long>();
            long long zombie_row = meta.at("zombie_row").get<long long>();

            fout << csv_escape(case_id) << ","
                 << zombie_wave << ","
                 << zombie_row << ",";

            try {
                json config_json = item.at("config");
                config_json["show_progress"] = false;
                ZomboniConfig config(config_json);

                double crush_rate = work(config, true);
                fout << crush_rate << ",\n";
            }
            catch (const std::exception& e) {
                fout << "," << csv_escape(e.what()) << "\n";
            }

            fout.flush();
        }
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
