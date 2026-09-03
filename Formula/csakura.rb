class Csakura < Formula
  desc "Sakura tree with falling petals for your terminal"
  homepage "https://github.com/realstrawhat/csakura"
  url "https://github.com/realstrawhat/csakura/archive/refs/tags/v2.1.0.tar.gz"
  sha256 "73ce581391c569a7d7c512189d14f8cb1994673de2c8df9239a5700d6368202f"
  license "MIT"
  head "https://github.com/realstrawhat/csakura.git", branch: "main"

  depends_on "pkgconf" => :build
  depends_on "ncurses"

  def install
    system "make"
    bin.install "csakura"
  end

  test do
    assert_match "csakura", shell_output("#{bin}/csakura -v")
  end
end
