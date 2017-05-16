MRuby::Gem::Specification.new('mruby-esp32-gpio') do |spec|
  spec.license = 'MIT'
  spec.authors = 'YAMAMOTO Masaya'

  spec.cc.include_paths << "#{build.root}/src"
end
