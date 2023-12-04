//  Copyright 2022 Filippo Savi <filssavi@gmail.com>
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "server_frontend/schema_validator.h"

schema_validator::schema_validator(nlohmann::json &chosen_schema_doc) {

     schema;
    valijson::SchemaParser parser;
    valijson::adapters::NlohmannJsonAdapter schema_adapter(chosen_schema_doc);
    parser.populateSchema(schema_adapter, schema);


}


bool schema_validator::validate(nlohmann::json &spec_file, std::string &error) {
    valijson::Validator validator;
    valijson::ValidationResults results;
    valijson::adapters::NlohmannJsonAdapter myTargetAdapter(spec_file);
    if (!validator.validate(schema, myTargetAdapter, &results)) {
        valijson::ValidationResults::Error err;
        unsigned int errorNum = 1;
        while (results.popError(err)) {

            std::string context;
            auto itr = err.context.begin();
            for (; itr != err.context.end(); itr++) {
                context += *itr;
            }

            error = "Error #" + std::to_string(errorNum) + "\n  context: " + context + "\n  desc:   " + err.description;

            ++errorNum;
        }
        return false;
    }
    return true;
}
