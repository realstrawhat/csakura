class Csakura < Formula
  desc "Sakura tree with falling petals for your terminal"
  homepage "https://github.com/realstrawhat/csakura"
  url "https://github.com/realstrawhat/csakura/archive/refs/tags/v2.0.0.tar.gz"
  sha256 "6ba89931a05b087c7979e2bcfcdd93f1cb03b1fa57ee405dbbebdf32a68e2348"
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
