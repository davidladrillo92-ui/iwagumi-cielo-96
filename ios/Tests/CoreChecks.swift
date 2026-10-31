import Foundation

@main struct CoreChecks {
    static func main() throws {
        var profile = try JSONDecoder().decode(CieloProfile.self,from:Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[1])))
        try profile.validate(); precondition(profile.computedChecksum == "dd129bb7")
        let encoded = try JSONEncoder().encode(profile)
        let copy = try JSONDecoder().decode(CieloProfile.self,from:encoded); precondition(copy == profile)
        precondition(profile.form(generation:14)["generation"] == "14")
        precondition(profile.form(generation:14)["checksum"] == "dd129bb7")
        for host in ["192.168.0.25","10.0.0.1","172.16.0.1"] { _ = try LocalAddress.url(host) }
        for host in ["8.8.8.8","127.0.0.1","192.168.0.25/api/save","user@192.168.0.25","192.168.0.25:80","example.com","192.168.00.25"] {
            do { _ = try LocalAddress.url(host); fatalError("Accepted public/malformed host") } catch CieloError.invalidHost {} }
        profile.lights.chihiros.points[1][0] = profile.lights.chihiros.points[0][0];profile.seal()
        do { try profile.validate();fatalError("Accepted duplicate time") } catch CieloError.invalidProfile {}
        print("PASS native core: import/export/checksum/generation, invalid profiles and private IPv4 boundaries")
    }
}
