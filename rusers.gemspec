
lib = File.expand_path('../lib', __FILE__)
$:.unshift lib unless $:.include?(lib)

require 'rusers/version'

Gem::Specification.new do |s|
  s.name          = 'rusers'
  s.version       = RemoteUsers::VERSION
  s.platform      = Gem::Platform::RUBY
  s.summary       = ''
  s.description   = ''
  s.authors       = [ 'Nathan Currier' ]
  s.email         = 'ride@cs.colostate.edu'
  s.files         = `git ls-files README.md LICENSE.md ext lib bin`.split
  s.homepage      = 'https://rideliner.net'
  s.license       = 'Boost'
  s.extensions    = %w[ ext/rusers/extconf.rb ]
  s.bindir        = 'bin'
  s.executables   = `git ls-files -- bin/*`.split.map { |f| File.basename(f) }
  s.require_paths = [ 'lib' ]
end
