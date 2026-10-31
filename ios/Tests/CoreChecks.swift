import Foundation

@main struct CoreChecks {
    static func main() throws {
        struct LampFixture:Decodable { let name:String;let pv:Bool;let fresh:Bool;let state:ModeState;let expected:LampPresentation }
        let fixturesURL=URL(fileURLWithPath:CommandLine.arguments[1]).deletingLastPathComponent().deletingLastPathComponent().appendingPathComponent("Tests/LampPresentationFixtures.json")
        let fixtures=try JSONDecoder().decode([LampFixture].self,from:Data(contentsOf:fixturesURL))
        for fixture in fixtures {
            precondition(fixture.state.presentation(pv:fixture.pv,fresh:fixture.fresh) == fixture.expected,"Lamp state differs: \(fixture.name)")
        }
        print("PASS \(fixtures.count) native lamp-state fixtures shared with the web contract")
        var profile = try JSONDecoder().decode(CieloProfile.self,from:Data(contentsOf:URL(fileURLWithPath:CommandLine.arguments[1])))
        try profile.validate(); precondition(profile.computedChecksum == "dd129bb7")
        let encoded = try JSONEncoder().encode(profile)
        let copy = try JSONDecoder().decode(CieloProfile.self,from:encoded); precondition(copy == profile)
        precondition(profile.form(generation:14)["generation"] == "14")
        precondition(profile.form(generation:14)["checksum"] == "dd129bb7")
        for host in ["192.168.0.25","10.0.0.1","172.16.0.1"] { _ = try LocalAddress.url(host) }
        for host in ["8.8.8.8","127.0.0.1","192.168.0.25/api/save","user@192.168.0.25","192.168.0.25:80","example.com","192.168.00.25"] {
            do { _ = try LocalAddress.url(host); fatalError("Accepted public/malformed host") } catch CieloError.invalidHost {} }
        // At every stored keyframe, preview must equal that exact keyframe.
        for pv in [false,true] {
            for row in (pv ? profile.lights.pezverde.points : profile.lights.chihiros.points) {
                precondition(profile.preview(minute:row[0],pv:pv) == Array(row.dropFirst()))
            }
        }
        var sample=profile
        sample.lights.chihiros.points=[[10,0,100,2],[20,5,95,1],[1440,0,0,0]]
        precondition(sample.preview(minute:0,pv:false) == [0,0,0])
        precondition(sample.preview(minute:15,pv:false) == [3,97,1])
        precondition(sample.preview(minute:1440,pv:false) == [0,0,0])
        precondition(sample.preview(minute:1441,pv:false) == [0,0,0])
        sample.seal()
        precondition(sample.editLimits(pv:false,index:1)?.minute == 11...1439)
        precondition(sample.editLimits(pv:false,index:2)?.minute == 1440...1440)
        precondition(sample.editLimits(pv:false,index:2)?.channelsLocked == true)
        precondition(profile.editLimits(pv:true,index:0)?.brightnessLocked == true)
        precondition(profile.editLimits(pv:true,index:profile.lights.pezverde.points.count-1)?.brightnessLocked == true)
        precondition(profile.editLimits(pv:true,index:-1) == nil)
        profile.lights.chihiros.points[1][0] = profile.lights.chihiros.points[0][0];profile.seal()
        do { try profile.validate();fatalError("Accepted duplicate time") } catch CieloError.invalidProfile {}
        print("PASS native core: import/export/checksum/generation, invalid profiles and private IPv4 boundaries")
    }
}
