import Foundation

enum CieloError: LocalizedError {
    case invalidHost, invalidProfile, unavailable, changed, server(String)
    var errorDescription: String? {
        switch self {
        case .invalidHost: return "Introduce una IPv4 privada, por ejemplo 192.168.0.25."
        case .invalidProfile: return "El perfil no es válido. Se conservan las curvas anteriores."
        case .unavailable: return "Sin respuesta del ATOM. Comprueba la red local y vuelve a consultar."
        case .changed: return "La conexión o el perfil ha cambiado. Vuelve a consultar antes de guardar."
        case .server(let message): return message
        }
    }
}

enum LocalAddress {
    static func url(_ host: String) throws -> URL {
        let parts = host.split(separator: ".", omittingEmptySubsequences: false)
        guard parts.count == 4 else { throw CieloError.invalidHost }
        let values = parts.compactMap { Int($0) }
        guard values.count == 4, values.allSatisfy({ (0...255).contains($0) }),
              zip(parts, values).allSatisfy({ String($0.1) == String($0.0) }),
              values[0] == 10 || (values[0] == 172 && (16...31).contains(values[1])) || (values[0] == 192 && values[1] == 168),
              let url = URL(string: "http://" + host) else { throw CieloError.invalidHost }
        return url
    }
}

struct LightCurve: Codable, Equatable { var points: [[Int]]; var unit: String }
struct ProfileLights: Codable, Equatable { var pezverde: LightCurve; var chihiros: LightCurve }
struct Integrity: Codable, Equatable { var algorithm: String; var checksum: String }
struct CieloProfile: Codable, Equatable {
    var schema = "iwagumi-cielo-profile"
    var schema_version = 2
    var curve_unit = "minute"
    var name = "CIELO · Perfil"
    var lights: ProfileLights
    var integrity: Integrity

    static func wire(_ rows: [[Int]]) -> String { rows.map { $0.map(String.init).joined(separator: ",") }.joined(separator: ";") }
    var computedChecksum: String {
        let input = Self.wire(lights.pezverde.points) + "|" + Self.wire(lights.chihiros.points)
        let hash = input.utf8.reduce(UInt32(2166136261)) { ($0 ^ UInt32($1)) &* 16777619 }
        return String(format: "%08x", hash)
    }
    mutating func seal() { integrity = Integrity(algorithm: "fnv1a32", checksum: computedChecksum) }
    func validate() throws {
        guard schema == "iwagumi-cielo-profile", schema_version == 2, curve_unit == "minute",
              lights.pezverde.unit == "rgb255-plus-brightness", lights.chihiros.unit == "percent" else { throw CieloError.invalidProfile }
        for (rows, columns, limit) in [(lights.pezverde.points, 5, 255), (lights.chihiros.points, 4, 100)] {
            guard (2...64).contains(rows.count), rows.allSatisfy({ row in
                row.count == columns && (0...1440).contains(row[0]) && row[1...3].allSatisfy({ (0...limit).contains($0) }) && (columns == 4 || (0...100).contains(row[4]))
            }), zip(rows, rows.dropFirst()).allSatisfy({ pair in pair.0[0] < pair.1[0] }), rows.last?[0] == 1440 else { throw CieloError.invalidProfile }
        }
        guard lights.pezverde.points.first?[4] == 0, lights.pezverde.points.last?[4] == 0,
              lights.chihiros.points.last?.dropFirst().allSatisfy({ $0 == 0 }) == true,
              integrity.algorithm == "fnv1a32", integrity.checksum.lowercased() == computedChecksum else { throw CieloError.invalidProfile }
    }
    static func parseWire(_ text: String) throws -> [[Int]] {
        try text.split(separator: ";", omittingEmptySubsequences: false).map { row in
            try row.split(separator: ",", omittingEmptySubsequences: false).map { cell in
                guard let n = Int(cell), String(n) == cell else { throw CieloError.invalidProfile }; return n
            }
        }
    }
    static func from(_ status: AtomStatus) throws -> Self {
        guard let pv = status.points, let ch = status.chiPoints else { throw CieloError.invalidProfile }
        var profile = Self(lights: ProfileLights(pezverde: LightCurve(points: try parseWire(pv), unit: "rgb255-plus-brightness"), chihiros: LightCurve(points: try parseWire(ch), unit: "percent")), integrity: Integrity(algorithm: "fnv1a32", checksum: ""))
        profile.seal(); try profile.validate(); return profile
    }
    func form(generation: Int) -> [String: String] {
        ["schema_version":"2", "generation":String(generation), "checksum":computedChecksum,
         "pv":Self.wire(lights.pezverde.points), "ch":Self.wire(lights.chihiros.points)]
    }
}

struct WorkLight: Decodable {
    var active: Bool?; var preparing: Bool?; var can_start: Bool?; var phase: String?
    var remaining_sec: Int?; var unavailable_reason: String?
}
struct ModeState: Decodable {
    var pv_link_connected: Bool?; var ch_link_connected: Bool?
    var pv_waiting_off_check: Bool?; var pv_off_obligation: Bool?
    var pv_consecutive_errors: Int?; var ch_consecutive_errors: Int?
    var pv_online_recent: Bool?; var ch_online_recent: Bool?
}
struct AtomHealth: Decodable { var boot_ready: Bool?; var current_critical: Bool?; var worker_unresponsive: Bool? }
struct AtomStatus: Decodable {
    var firmware: String?; var boot_id: Int?; var clockValid: Bool?; var clock: String?
    var rgb: [Int]?; var brightness: Int?; var chLiveRgb: [Int]?
    var profileGeneration: Int?; var points: String?; var chiPoints: String?
    var cieloV4: ModeState?; var work_light: WorkLight?; var health: AtomHealth?
}
