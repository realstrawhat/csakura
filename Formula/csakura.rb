class Csakura < Formula
  desc "A sakura tree with falling petals for your terminal"
  homepage "https://github.com/realstrawhat/csakura"
  license "MIT"
  head "https://github.com/realstrawhat/csakura.git", branch: "main"

  depends_on "ncurses"

  def install
    system "make", "PREFIX=#{prefix}"
    bin.install "csakura"
  end

  test do
    assert_match "csakura", shell_output("#{bin}/csakura -v")
  end
end
