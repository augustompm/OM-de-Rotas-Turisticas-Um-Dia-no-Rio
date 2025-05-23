#include "utils.hpp"
#include "models.hpp"  
#include "hypervolume.hpp"  
#include <fstream>
#include <sstream>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <unordered_map>

namespace tourist {
namespace utils {

// Inicialização das variáveis estáticas
std::vector<std::vector<double>> TransportMatrices::car_distances;
std::vector<std::vector<double>> TransportMatrices::walk_distances;
std::vector<std::vector<double>> TransportMatrices::car_times;
std::vector<std::vector<double>> TransportMatrices::walk_times;
std::unordered_map<std::string, size_t> TransportMatrices::attraction_indices;
std::vector<std::string> TransportMatrices::attraction_names;
bool TransportMatrices::matrices_loaded = false;

// Mapa para normalização de nomes de atrações
static std::unordered_map<std::string, std::string> attraction_name_mapping;

// Função para normalizar strings (remover acentos, espaços extras, etc.)
std::string normalizeString(const std::string& str) {
    std::string result = str;
    
    // Converter para minúsculas
    std::transform(result.begin(), result.end(), result.begin(), 
                  [](unsigned char c) { return std::tolower(c); });
    
    // Remover caracteres não alfanuméricos e substituir por espaços
    for (auto& c : result) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != ' ') {
            c = ' ';
        }
    }
    
    // Remover espaços múltiplos
    auto newEnd = std::unique(result.begin(), result.end(),
                             [](char a, char b) {
                                 return a == ' ' && b == ' ';
                             });
    result.erase(newEnd, result.end());
    
    // Trim espaços no início e fim
    if (!result.empty()) {
        size_t start = result.find_first_not_of(' ');
        if (start != std::string::npos) {
            size_t end = result.find_last_not_of(' ');
            result = result.substr(start, end - start + 1);
        } else {
            result = "";
        }
    }
    
    return result;
}

// Função auxiliar para criar o mapeamento de nomes
void createAttractionNameMapping() {
    if (!TransportMatrices::matrices_loaded) {
        std::cerr << "Erro: matrizes de transporte não carregadas antes de criar mapeamento." << std::endl;
        return;
    }
    
    // Limpar mapeamento existente
    attraction_name_mapping.clear();
    
    std::cout << "Criando mapeamento de nomes de atrações..." << std::endl;
    
    // Criar mapeamento para cada atração nas matrizes
    for (const auto& name : TransportMatrices::attraction_names) {
        std::string normalized = normalizeString(name);
        attraction_name_mapping[normalized] = name;
        
        // Também adicionar versão sem espaços
        std::string no_spaces = normalized;
        no_spaces.erase(std::remove(no_spaces.begin(), no_spaces.end(), ' '), no_spaces.end());
        attraction_name_mapping[no_spaces] = name;
    }
    
    std::cout << "Mapeamento de nomes criado: " << attraction_name_mapping.size() << " entradas." << std::endl;
}

// Função para encontrar o nome correspondente na matriz
std::string findMatrixName(const std::string& original_name) {
    static bool mapping_created = false;
    
    // Create the mapping on the first call
    if (!mapping_created && TransportMatrices::matrices_loaded) {
        createAttractionNameMapping();
        mapping_created = true;
    }
    
    // Direct manual mappings for problematic attractions
    static const std::unordered_map<std::string, std::string> manual_mappings = {
        {"Museu do Amanhã", "Museu do Amanh"},
        {"Escadaria Selarón", "Escadaria Selaron"},
        {"Pedra da Gávea", "Pedra da Gavea"},
        {"Pedra do Telégrafo", "Pedra do Telegrafo"},
        {"Morro Dois Irmãos", "Morro Dois Irmaos"},
        {"Mosteiro De São Bento", "Mosteiro De Sao Bento"},
        {"Real Gabinete Português Da Leitura", "Real Gabinete Portugues Da Leitura"},
        {"Centro Cultural Jerusalém", "Centro Cultural Jerusalem"},
        {"Ilha Da Gigóia", "Ilha Da Gigoia"}
    };
    
    // Check for direct manual mapping first (without logs)
    auto it = manual_mappings.find(original_name);
    if (it != manual_mappings.end()) {
        return it->second;
    }
    
    // Normalize the original name
    std::string normalized = normalizeString(original_name);
    
    // Check for direct match (without logs)
    if (attraction_name_mapping.find(normalized) != attraction_name_mapping.end()) {
        return attraction_name_mapping[normalized];
    }
    
    // Try version without spaces
    std::string no_spaces = normalized;
    no_spaces.erase(std::remove(no_spaces.begin(), no_spaces.end(), ' '), no_spaces.end());
    if (attraction_name_mapping.find(no_spaces) != attraction_name_mapping.end()) {
        return attraction_name_mapping[no_spaces];
    }
    
    // Try fuzzy matching - find the best partial match
    std::string best_match;
    size_t best_score = 0;
    
    for (const auto& [norm_name, matrix_name] : attraction_name_mapping) {
        // Calculate a simple matching score
        size_t len1 = norm_name.length();
        size_t len2 = normalized.length();
        size_t common_len = 0;
        
        for (size_t i = 0; i < std::min(len1, len2); ++i) {
            if (i < len1 && i < len2 && norm_name[i] == normalized[i]) {
                common_len++;
            }
        }
        
        size_t score = (common_len * 100) / std::max(len1, len2);
        
        if (score > best_score && score > 60) {  // Require at least 60% match
            best_score = score;
            best_match = matrix_name;
        }
    }
    
    if (!best_match.empty()) {
        return best_match;
    }
    
    // No match found
    return original_name;
}

// Função auxiliar para normalizar nomes (trim e remover caracteres especiais)
std::string normalizeAttractionName(const std::string& name) {
    // Primeiro tenta encontrar o nome correspondente na matriz
    std::string matrix_name = findMatrixName(name);
    if (matrix_name != name) {
        return matrix_name;  // Se encontrou uma correspondência, use-a
    }
    
    // Caso contrário, use a normalização simples original
    std::string normalized = name;
    // Trim whitespace
    normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
    normalized.erase(normalized.find_last_not_of(" \t\r\n") + 1);
    
    // Remover caracteres especiais ou não-ASCII se necessário
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(),
                                   [](unsigned char c) {
                                       return !std::isprint(c) || c > 127;
                                   }),
                     normalized.end());
    
    return normalized;
}

// Implementações da classe Transport
double Transport::getDistance(const std::string& from, const std::string& to, TransportMode mode) {
    if (!TransportMatrices::matrices_loaded) {
        throw std::runtime_error("Transport matrices not loaded. Call loadTransportMatrices first.");
    }
    
    // Normalizar nomes para busca consistente
    std::string from_normalized = normalizeAttractionName(from);
    std::string to_normalized = normalizeAttractionName(to);
    
    auto from_it = TransportMatrices::attraction_indices.find(from_normalized);
    auto to_it = TransportMatrices::attraction_indices.find(to_normalized);
    
    // Se não encontrar diretamente, tentar variantes do nome
    if (from_it == TransportMatrices::attraction_indices.end()) {
        // Tentar versão sem espaços
        std::string from_no_spaces = from_normalized;
        from_no_spaces.erase(std::remove_if(from_no_spaces.begin(), from_no_spaces.end(), ::isspace), from_no_spaces.end());
        from_it = TransportMatrices::attraction_indices.find(from_no_spaces);
        
        // Debug: imprimir nomes disponíveis
        if (from_it == TransportMatrices::attraction_indices.end()) {
            std::cerr << "Atração não encontrada: '" << from_normalized << "'" << std::endl;
        }
    }
    
    if (to_it == TransportMatrices::attraction_indices.end()) {
        // Tentar versão sem espaços
        std::string to_no_spaces = to_normalized;
        to_no_spaces.erase(std::remove_if(to_no_spaces.begin(), to_no_spaces.end(), ::isspace), to_no_spaces.end());
        to_it = TransportMatrices::attraction_indices.find(to_no_spaces);
    }
    
    if (from_it == TransportMatrices::attraction_indices.end() || 
        to_it == TransportMatrices::attraction_indices.end()) {
        // Gerar valor de distância padrão alto para penalizar rotas com atrações ausentes
        // Em vez de lançar exceção, retornamos um valor alto
        double penalty_distance = 20000.0;  // 20 km de penalidade
        return penalty_distance;
    }
    
    size_t from_idx = from_it->second;
    size_t to_idx = to_it->second;
    
    // Verificar limites para evitar acesso inválido
    if (from_idx >= TransportMatrices::car_distances.size() ||
        to_idx >= TransportMatrices::car_distances[from_idx].size()) {
        double penalty_distance = 20000.0;  // 20 km de penalidade
        return penalty_distance;
    }
    
    if (mode == TransportMode::WALK) {
        return TransportMatrices::walk_distances[from_idx][to_idx];
    } else {
        return TransportMatrices::car_distances[from_idx][to_idx];
    }
}

double Transport::getTravelTime(const std::string& from, const std::string& to, TransportMode mode) {
    if (!TransportMatrices::matrices_loaded) {
        throw std::runtime_error("Transport matrices not loaded. Call loadTransportMatrices first.");
    }
    
    // Normalizar nomes para busca consistente
    std::string from_normalized = normalizeAttractionName(from);
    std::string to_normalized = normalizeAttractionName(to);
    
    auto from_it = TransportMatrices::attraction_indices.find(from_normalized);
    auto to_it = TransportMatrices::attraction_indices.find(to_normalized);
    
    // Se não encontrar diretamente, tentar variantes do nome
    if (from_it == TransportMatrices::attraction_indices.end()) {
        // Tentar versão sem espaços
        std::string from_no_spaces = from_normalized;
        from_no_spaces.erase(std::remove_if(from_no_spaces.begin(), from_no_spaces.end(), ::isspace), from_no_spaces.end());
        from_it = TransportMatrices::attraction_indices.find(from_no_spaces);
    }
    
    if (to_it == TransportMatrices::attraction_indices.end()) {
        // Tentar versão sem espaços
        std::string to_no_spaces = to_normalized;
        to_no_spaces.erase(std::remove_if(to_no_spaces.begin(), to_no_spaces.end(), ::isspace), to_no_spaces.end());
        to_it = TransportMatrices::attraction_indices.find(to_no_spaces);
    }
    
    if (from_it == TransportMatrices::attraction_indices.end() || 
        to_it == TransportMatrices::attraction_indices.end()) {
        // Retornar valor de tempo alto como penalidade
        double penalty_time = 60.0;  // 60 minutos como penalidade
        return penalty_time;
    }
    
    size_t from_idx = from_it->second;
    size_t to_idx = to_it->second;
    
    // Verificar limites para evitar acesso inválido
    if (from_idx >= TransportMatrices::car_times.size() ||
        to_idx >= TransportMatrices::car_times[from_idx].size()) {
        double penalty_time = 60.0;  // 60 minutos como penalidade
        return penalty_time;
    }
    
    if (mode == TransportMode::WALK) {
        return TransportMatrices::walk_times[from_idx][to_idx];
    } else {
        return TransportMatrices::car_times[from_idx][to_idx];
    }
}

double Transport::getTravelCost(const std::string& from, const std::string& to, TransportMode mode) {
    if (mode == TransportMode::WALK) {
        return 0.0; // Caminhada não tem custo
    } else {
        try {
            // Para carro, custo = R$6 por km (distância em metros / 1000 * 6)
            double distance_m = getDistance(from, to, TransportMode::CAR);
            double distance_km = distance_m / 1000.0;
            return distance_km * Config::COST_CAR_PER_KM;
        } catch (const std::exception& e) {
            // Retornar um valor de penalidade para não bloquear a execução
            return 100.0; // R$100 como penalidade
        }
    }
}

TransportMode Transport::determinePreferredMode(const std::string& from, const std::string& to) {
    try {
        double walk_time = getTravelTime(from, to, TransportMode::WALK);
        // Verificação mais estrita: só considera caminhada se for menor que o limite de preferência
        if (walk_time <= Config::WALK_TIME_PREFERENCE) {
            return TransportMode::WALK;
        } else {
            return TransportMode::CAR;
        }
    } catch (const std::exception& e) {
        // Em caso de erro, usar o modo padrão
        return TransportMode::CAR;
    }
}

std::string Transport::getModeString(TransportMode mode) {
    return (mode == TransportMode::WALK) ? "Walk" : "Car";
}

std::string Transport::formatTime(double minutes) {
    int total_minutes = static_cast<int>(minutes);
    int hours = total_minutes / 60;
    int mins = total_minutes % 60;
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << mins;
    return ss.str();
}

// Implementação da função de hipervolume
double Metrics::calculateHypervolume(const std::vector<Solution>& solutions, 
                                   const std::vector<double>& reference_point) {
    return HypervolumeCalculator::calculate(solutions, reference_point);
}

double Metrics::calculateSpread(const std::vector<Solution>& solutions) {
    if (solutions.size() < 2) return 0.0;
    
    std::vector<double> distances;
    distances.reserve(solutions.size() - 1);
    
    for (size_t i = 0; i < solutions.size() - 1; ++i) {
        double dist = 0.0;
        const auto& obj1 = solutions[i].getObjectives();
        const auto& obj2 = solutions[i+1].getObjectives();
        
        for (size_t j = 0; j < obj1.size(); ++j) {
            dist += std::pow(obj1[j] - obj2[j], 2);
        }
        distances.push_back(std::sqrt(dist));
    }
    
    double avg_dist = std::accumulate(distances.begin(), distances.end(), 0.0) / distances.size();
    double spread = 0.0;
    
    for (double dist : distances) {
        spread += std::abs(dist - avg_dist);
    }
    
    return spread / (distances.size() * avg_dist);
}

double Metrics::calculateCoverage(const std::vector<Solution>& solutions1,
                                const std::vector<Solution>& solutions2) {
    if (solutions2.empty()) return 1.0;
    if (solutions1.empty()) return 0.0;
    
    int dominated_count = 0;
    for (const auto& sol2 : solutions2) {
        for (const auto& sol1 : solutions1) {
            if (sol1.dominates(sol2)) {
                dominated_count++;
                break;
            }
        }
    }
    
    return static_cast<double>(dominated_count) / solutions2.size();
}

// Implementações da classe Parser
std::vector<Attraction> Parser::loadAttractions(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open attractions file: " + filename);
    }

    std::vector<Attraction> attractions;
    std::string line;
    
    std::getline(file, line); // Skip header line
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        auto parts = split(line, ';');
        if (parts.size() < 7) {  // Update to check for all required fields including neighborhood
            std::cerr << "Warning: Invalid line format ignored: " << line << std::endl;
            continue;
        }
        
        try {
            auto coords = parseCoordinates(parts[2]);
            int visitTime = std::stoi(parts[3]);
            double cost = std::stod(parts[4]);
            int openingTime = std::stoi(parts[5]);
            int closingTime = std::stoi(parts[6]);
            
            attractions.emplace_back(
                parts[0],                      // name
                parts[1],                      // neighborhood
                coords.first,                  // latitude
                coords.second,                 // longitude
                visitTime,                     // visit time
                cost,                          // cost
                openingTime,                   // opening time
                closingTime                    // closing time
            );
        } catch (const std::exception& e) {
            std::cerr << "Error processing attraction: " << e.what() << std::endl;
        }
    }
    
    return attractions;
}

bool Parser::loadTransportMatrices(const std::string& car_distances_file,
                                   const std::string& walk_distances_file,
                                   const std::string& car_times_file,
                                   const std::string& walk_times_file) {
    try {
        // Limpar dados anteriores se existirem
        TransportMatrices::car_distances.clear();
        TransportMatrices::walk_distances.clear();
        TransportMatrices::car_times.clear();
        TransportMatrices::walk_times.clear();
        TransportMatrices::attraction_indices.clear();
        TransportMatrices::attraction_names.clear();
        
        TransportMatrices::car_distances = parseMatrixFile(car_distances_file);
        TransportMatrices::walk_distances = parseMatrixFile(walk_distances_file);
        TransportMatrices::car_times = parseMatrixFile(car_times_file);
        TransportMatrices::walk_times = parseMatrixFile(walk_times_file);
        
        if (TransportMatrices::car_distances.empty() || 
            TransportMatrices::walk_distances.empty() ||
            TransportMatrices::car_times.empty() || 
            TransportMatrices::walk_times.empty()) {
            std::cerr << "Error: One or more matrix files are empty" << std::endl;
            return false;
        }
        
        // Extrair os nomes das atrações do arquivo de distâncias de carro
        std::ifstream file(car_distances_file);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << car_distances_file << std::endl;
            return false;
        }
        
        std::string header;
        std::getline(file, header);
        
        // Remover possível BOM UTF-8
        const char* utf8_bom = "\xEF\xBB\xBF";
        if (header.size() >= 3 && header.substr(0, 3) == utf8_bom) {
            header = header.substr(3);
        }
        
        // Separar nomes das atrações
        auto attraction_names = split(header, ';');
        if (!attraction_names.empty() && attraction_names[0].empty()) {
            // Primeira coluna vazia (label para nomes de linha)
            attraction_names.erase(attraction_names.begin());
        }
        
        // Processar nomes e indexá-los
        for (size_t i = 0; i < attraction_names.size(); ++i) {
            std::string& name = attraction_names[i];
            
            // Normalizar o nome
            name = normalizeAttractionName(name);
            
            if (!name.empty()) {
                // Indexar o nome normalizado
                TransportMatrices::attraction_indices[name] = i;
                
                // Também indexar versão sem espaços para maior robustez
                std::string name_no_spaces = name;
                name_no_spaces.erase(std::remove_if(name_no_spaces.begin(),
                                                  name_no_spaces.end(),
                                                  ::isspace),
                                   name_no_spaces.end());
                if (name_no_spaces != name) {
                    TransportMatrices::attraction_indices[name_no_spaces] = i;
                }
            }
        }
        
        TransportMatrices::attraction_names = attraction_names;
        
        // Imprimir informações de diagnóstico
        std::cout << "Loaded " << attraction_names.size() << " attractions." << std::endl;
        std::cout << "Matrix dimensions: " << TransportMatrices::car_distances.size() << "x";
        if (!TransportMatrices::car_distances.empty()) {
            std::cout << TransportMatrices::car_distances[0].size();
        }
        std::cout << std::endl;
        
        // Guardar status de carregamento
        TransportMatrices::matrices_loaded = true;
        
        // Criar mapeamento de atrações
        createAttractionNameMapping();
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading matrices: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::vector<double>> Parser::parseMatrixFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open matrix file: " + filename);
    }
    
    std::vector<std::vector<double>> matrix;
    std::string line;
    
    // Pula a linha de cabeçalho
    std::getline(file, line);
    
    // Ler cada linha (cada atração de origem)
    while (std::getline(file, line)) {
        std::vector<double> row;
        
        // Remover possível BOM UTF-8 da primeira linha
        if (matrix.empty() && line.size() >= 3) {
            const char* utf8_bom = "\xEF\xBB\xBF";
            if (line.substr(0, 3) == utf8_bom) {
                line = line.substr(3);
            }
        }
        
        auto parts = split(line, ';');
        
        if (parts.size() <= 1) {
            std::cerr << "Warning: Invalid line in matrix file: " << line << std::endl;
            continue;
        }
        
        row.reserve(parts.size() - 1);
        
        // O primeiro elemento é o nome da atração, pulamos
        for (size_t i = 1; i < parts.size(); ++i) {
            try {
                // Substituir vírgulas por pontos para números em formato brasileiro
                std::string value_str = parts[i];
                std::replace(value_str.begin(), value_str.end(), ',', '.');
                
                // Converter para double
                row.push_back(std::stod(value_str));
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error parsing value '" << parts[i] 
                          << "': " << e.what() << ". Using 0.0 instead." << std::endl;
                row.push_back(0.0); // Valor padrão em caso de erro
            }
        }
        
        matrix.push_back(row);
    }
    
    return matrix;
}

std::pair<double, double> Parser::parseCoordinates(const std::string& coords) {
    auto parts = split(coords, ',');
    if (parts.size() != 2) {
        throw std::runtime_error("Invalid coordinates format: " + coords);
    }
    
    try {
        // Substituir vírgulas por pontos para números em formato brasileiro
        std::string lat_str = parts[0];
        std::string lon_str = parts[1];
        std::replace(lat_str.begin(), lat_str.end(), ',', '.');
        std::replace(lon_str.begin(), lon_str.end(), ',', '.');
        
        return {
            std::stod(lat_str),  // latitude
            std::stod(lon_str)   // longitude
        };
    } catch (const std::exception& e) {
        throw std::runtime_error("Error parsing coordinates: " + std::string(e.what()));
    }
}

std::vector<std::string> Parser::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(s);
    
    while (std::getline(token_stream, token, delimiter)) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\r\n"));
        token.erase(token.find_last_not_of(" \t\r\n") + 1);
        tokens.push_back(token);
    }
    
    return tokens;
}

} // namespace utils
} // namespace tourist